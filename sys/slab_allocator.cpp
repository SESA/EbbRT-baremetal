#include <boost/container/static_vector.hpp>

#include <sys/debug.hpp>
#include <sys/ebb_allocator.hpp>
#include <sys/explicitly_constructed.hpp>
#include <sys/local_id_map.hpp>
#include <sys/slab_allocator.hpp>
#include <sys/page_allocator.hpp>

using namespace ebbrt;

namespace {
size_t slab_order(size_t size, size_t max_order, unsigned frac) {
  size_t order;
  if (fls(size - 1) < PAGE_SHIFT)
    order = 0;
  else
    order = fls(size - 1) - PAGE_SHIFT + 1;

  while (order <= max_order) {
    auto slab_size = PAGE_SIZE << order;
    auto objects = slab_size / size;
    auto waste = slab_size - (objects * size);

    if (waste * frac <= slab_size)
      break;

    ++order;
  }

  return order;
}

// These numbers come from the defunct SLQB allocator in Linux, may need tuning
size_t calculate_order(size_t size) {
  auto order = slab_order(size, 1, 4);
  if (order <= 1)
    return order;

  order = slab_order(size, PageAllocator::MAX_ORDER, 0);
  kbugon(order > PageAllocator::MAX_ORDER, "Request for too big a slab\n");
  return order;
}

size_t calculate_freebatch(size_t size) {
  return std::max(4 * PAGE_SIZE / size,
                  std::min(size_t(256), 64 * PAGE_SIZE / size));
}

pfn_t addr_to_slab_pfn(void *addr, size_t order) {
  auto pfn = pfn_down(addr);
  return pfn_t(align_down(uintptr_t(pfn), 1 << order));
}

explicitly_constructed<SlabAllocatorRoot> slab_root_allocator;
boost::container::static_vector<SlabAllocatorNode, MAX_NUMA_NODES>
slab_root_allocator_node_allocators;
boost::container::static_vector<SlabAllocator, MAX_NUM_CPUS>
slab_root_allocator_cpu_allocators;

explicitly_constructed<SlabAllocatorRoot> slab_node_allocator;
boost::container::static_vector<SlabAllocatorNode, MAX_NUMA_NODES>
slab_node_allocator_node_allocators;
boost::container::static_vector<SlabAllocator, MAX_NUM_CPUS>
slab_node_allocator_cpu_allocators;

explicitly_constructed<SlabAllocatorRoot> slab_cpu_allocator;
boost::container::static_vector<SlabAllocatorNode, MAX_NUMA_NODES>
slab_cpu_allocator_node_allocators;
boost::container::static_vector<SlabAllocator, MAX_NUM_CPUS>
slab_cpu_allocator_cpu_allocators;
}

void ebbrt::slab_init() {
  slab_root_allocator.construct(sizeof(SlabAllocatorRoot),
                                alignof(SlabAllocatorRoot));
  slab_node_allocator.construct(sizeof(SlabAllocatorNode),
                                alignof(SlabAllocatorNode));
  slab_cpu_allocator.construct(sizeof(SlabAllocator), alignof(SlabAllocator));

  for (unsigned i = 0; i < numa_nodes.size(); ++i) {
    slab_root_allocator_node_allocators.emplace_back(*slab_root_allocator,
                                                     nid_t(i));
    slab_root_allocator->node_allocators[i] =
        &slab_root_allocator_node_allocators[i];

    slab_node_allocator_node_allocators.emplace_back(*slab_node_allocator,
                                                     nid_t(i));
    slab_node_allocator->node_allocators[i] =
        &slab_node_allocator_node_allocators[i];

    slab_cpu_allocator_node_allocators.emplace_back(*slab_cpu_allocator,
                                                    nid_t(i));
    slab_cpu_allocator->node_allocators[i] =
        &slab_cpu_allocator_node_allocators[i];
  }

  for (unsigned i = 0; i < cpus.size(); ++i) {
    slab_root_allocator_cpu_allocators.emplace_back(*slab_root_allocator);
    slab_root_allocator->cpu_allocators[i] =
        std::unique_ptr<SlabAllocator>(&slab_root_allocator_cpu_allocators[i]);

    slab_node_allocator_cpu_allocators.emplace_back(*slab_cpu_allocator);
    slab_node_allocator->cpu_allocators[i] =
        std::unique_ptr<SlabAllocator>(&slab_node_allocator_cpu_allocators[i]);

    slab_cpu_allocator_cpu_allocators.emplace_back(*slab_cpu_allocator);
    slab_cpu_allocator->cpu_allocators[i] =
        std::unique_ptr<SlabAllocator>(&slab_cpu_allocator_cpu_allocators[i]);
  }
}

SlabCache::SlabCache(SlabAllocatorRoot &root)
    : root_(root), remote_check{ false } {}
SlabCache::~SlabCache() {
  kassert(object_list_.empty());
  kassert(remote_.list.empty());

  if (!partial_page_list_.empty()) {
    kprintf("Memory leak detected, slab partial page list is not empty\n");
  }
}

void *SlabCache::Alloc() {
retry:
  // first check the free list and return if we have a free object
  if (!object_list_.empty()) {
    auto &ret = object_list_.front();
    object_list_.pop_front();
    return ret.addr();
  }

  // no free object in lock list, potentially check the list of remotely freed
  // objects
  if (remote_check) {
    ClaimRemoteFreeList();

    while (object_list_.size() > root_.hiwater)
      FlushFreeList(root_.free_batch);

    goto retry;
  }

  // no free object in either list, try to get an object from a partial page
  if (!partial_page_list_.empty()) {
    auto &page = partial_page_list_.front();
    auto &page_slab_data = page.data.slab_data;
    kassert(page_slab_data.used < root_.num_objects_per_slab());
    // if this is the last allocation remove it from the list
    if (page_slab_data.used + 1 == root_.num_objects_per_slab()) {
      partial_page_list_.pop_front();
    }

    kassert(!page_slab_data.list->empty());

    ++page_slab_data.used;

    auto &object = page_slab_data.list->front();
    page_slab_data.list->pop_front();
    return object.addr();
  }

  return nullptr;
}

void SlabCache::AddSlab(pfn_t pfn) {
  auto page = pfn_to_page(pfn);
  kassert(page != nullptr);

  page->usage = page::Usage::SLAB_ALLOCATOR;

  auto &page_slab_data = page->data.slab_data;
  page_slab_data.member_hook.construct();
  page_slab_data.list.construct();
  page_slab_data.used = 0;

  // Initialize free list
  auto start = pfn_to_addr(pfn);
  auto end = pfn_to_addr(pfn) + root_.num_objects_per_slab() * root_.size;
  for (uintptr_t addr = start; addr < end; addr += root_.size) {
    // add object to per slab free list
    auto object = new (reinterpret_cast<void *>(addr)) free_object();
    page_slab_data.list->push_front(*object);

    auto addr_page = addr_to_page(addr);
    kassert(addr_page != nullptr);
    auto &addr_page_slab_data = addr_page->data.slab_data;
    addr_page_slab_data.cache = this;
  }

  partial_page_list_.push_front(*page);
}

void SlabCache::Free(void *p) {
  auto object = new (p) free_object();
  object_list_.push_front(*object);
  if (object_list_.size() > root_.hiwater) {
    FlushFreeList(root_.free_batch);
  }
}

void SlabCache::FlushFreeList(size_t amount) {
  size_t freed = 0;
  while (!object_list_.empty() && freed < amount) {
    auto &object = object_list_.front();
    object_list_.pop_front();
    auto obj_addr = object.addr();
    auto pfn = addr_to_slab_pfn(obj_addr, root_.order);
    auto page = pfn_to_page(pfn);
    kassert(page != nullptr);

    auto &page_slab_data = page->data.slab_data;

    if (page_slab_data.cache != this) {
      auto &allocator = root_.get_cpu_allocator();
      allocator.FreeRemote(obj_addr);
    } else {
      auto object = new (obj_addr) free_object();
      page_slab_data.list->push_front(*object);
      --page_slab_data.used;

      if (page_slab_data.used == 0) {
        if (root_.num_objects_per_slab() > 1) {
          partial_page_list_.erase(partial_page_list_.iterator_to(*page));
        }

        // free the page
        page_slab_data.member_hook.destruct();
        page_slab_data.list.destruct();
        page_allocator->Free(pfn, root_.order);
      } else if (page_slab_data.used + 1 == root_.num_objects_per_slab()) {
        partial_page_list_.push_front(*page);
      }
    }

    ++freed;
  }
}

void SlabCache::FlushFreeListAll() { FlushFreeList(size_t(-1)); }

void SlabCache::ClaimRemoteFreeList() {
  std::lock_guard<spinlock> lock{ remote_.lock };

  if (object_list_.empty()) {
    object_list_.swap(remote_.list);
  } else {
    object_list_.splice_after(object_list_.end(), remote_.list);
  }
}

EbbRef<SlabAllocator> SlabAllocator::Construct(size_t size) {
  auto id = ebb_allocator->AllocateLocal();
  auto allocator_root = new SlabAllocatorRoot(size);
  local_id_map->insert(std::make_pair(id, allocator_root));
  return EbbRef<SlabAllocator>{ id };
}

SlabAllocator &SlabAllocator::HandleFault(EbbId id) {
  SlabAllocatorRoot *allocator_root;
  {
    LocalIdMap::const_accessor accessor;
    auto found = local_id_map->find(accessor, id);
    kassert(found);
    allocator_root = boost::any_cast<SlabAllocatorRoot *>(accessor->second);
  }
  kassert(allocator_root != nullptr);
  auto &allocator = allocator_root->get_cpu_allocator();
  cache_ref(id, allocator);
  return allocator;
}

SlabAllocator::SlabAllocator(SlabAllocatorRoot &root)
    : cache_{ root }, remote_cache_{ nullptr } {}

void *SlabAllocator::operator new(size_t size, nid_t nid) {
  kassert(size == sizeof(SlabAllocator));
  if (nid == my_node()) {
    auto &allocator = slab_cpu_allocator->get_cpu_allocator();
    auto ret = allocator.Alloc();
    if (ret == nullptr) {
      throw std::bad_alloc();
    }
    return ret;
  }

  auto &allocator = slab_cpu_allocator->get_node_allocator(nid);
  auto ret = allocator.Alloc();
  if (ret == nullptr) {
    throw std::bad_alloc();
  }
  return ret;
}

void SlabAllocator::operator delete(void *p) {
  auto &allocator = slab_cpu_allocator->get_cpu_allocator();
  allocator.Free(p);
}

void *SlabAllocator::Alloc(nid_t nid) {
  if (nid == my_node()) {
    auto ret = cache_.Alloc();
    if (ret == nullptr) {
      auto pfn = page_allocator->Alloc(cache_.root_.order, nid);
      if (pfn == NO_PFN)
        return nullptr;
      cache_.AddSlab(pfn);
      ret = cache_.Alloc();
      kassert(ret != nullptr);
    }
    return ret;
  }

  auto &node_allocator = cache_.root_.get_node_allocator(nid);
  return node_allocator.Alloc();
}

void SlabAllocator::Free(void *p) {
  auto page = addr_to_page(p);
  kassert(page != nullptr);

  auto nid = page->nid;
  if (nid == my_node()) {
    cache_.Free(p);
  } else {
    FreeRemote(p);
  }
}

void SlabAllocator::FreeRemote(void *p) {
  auto page = addr_to_page(p);
  kassert(page != nullptr);

  auto &page_slab_data = page->data.slab_data;
  if (page_slab_data.cache != remote_cache_) {
    FlushRemoteList();
    remote_cache_ = page_slab_data.cache;
  }

  auto object = new (p) free_object;
  remote_list_.push_front(*object);

  if (remote_list_.size() > cache_.root_.free_batch) {
    FlushRemoteList();
  }
}

void SlabAllocator::FlushRemoteList() {
  if (remote_cache_ == nullptr)
    return;

  std::lock_guard<spinlock> lock(remote_cache_->remote_.lock);
  auto &list = remote_cache_->remote_.list;
  auto size = list.size();
  if (list.empty()) {
    list.swap(remote_list_);
  } else {
    list.splice_after(list.begin(), remote_list_);
  }
  auto flush_watermark = cache_.root_.free_batch;
  if (size < flush_watermark && list.size() >= flush_watermark) {
    // We crossed the watermark on this flush, mark the remote_check
    remote_cache_->remote_check = true;
  }
}

SlabAllocatorNode::SlabAllocatorNode(SlabAllocatorRoot &root, nid_t nid)
    : cache_{ root }, nid_{ nid } {}

void *SlabAllocatorNode::operator new(size_t size, nid_t nid) {
  kassert(size == sizeof(SlabAllocatorNode));
  if (nid == my_node()) {
    auto &allocator = slab_node_allocator->get_cpu_allocator();
    auto ret = allocator.Alloc();
    if (ret == nullptr) {
      throw std::bad_alloc();
    }
    return ret;
  }

  auto &allocator = slab_node_allocator->get_node_allocator(nid);
  auto ret = allocator.Alloc();
  if (ret == nullptr) {
    throw std::bad_alloc();
  }
  return ret;
}

void SlabAllocatorNode::operator delete(void *p) {
  auto &allocator = slab_node_allocator->get_cpu_allocator();
  allocator.Free(p);
}

void *SlabAllocatorNode::Alloc() {
  std::lock_guard<spinlock> lock(lock_);
  auto ret = cache_.Alloc();
  if (ret == nullptr) {
    auto pfn = page_allocator->Alloc(cache_.root_.order, nid_);
    if (pfn == NO_PFN)
      return nullptr;
    cache_.AddSlab(pfn);
    ret = cache_.Alloc();
    kassert(ret != nullptr);
  }
  return ret;
}

SlabAllocatorRoot::SlabAllocatorRoot(size_t size_in, size_t align_in)
    : align{ align_up(std::max(align_in, sizeof(void *)), sizeof(void *)) },
      size{ align_up(std::max(size_in, sizeof(void *)), align) },
      order{ calculate_order(size) }, free_batch{ calculate_freebatch(size) },
      hiwater{ free_batch * 4 } {
  std::fill(node_allocators.begin(), node_allocators.end(), nullptr);
  std::fill(cpu_allocators.begin(), cpu_allocators.end(), nullptr);
}

SlabAllocatorRoot::~SlabAllocatorRoot() {
  for (auto &cpu_allocator : cpu_allocators) {
    auto allocator = cpu_allocator.get();
    if (allocator != nullptr) {
      allocator->cache_.FlushFreeListAll();
      allocator->FlushRemoteList();
    }
  }

  for (auto &cpu_allocator : cpu_allocators) {
    auto allocator = cpu_allocator.get();
    if (allocator != nullptr) {
      allocator->cache_.ClaimRemoteFreeList();
      allocator->cache_.FlushFreeListAll();
    }
  }

  for (auto &node_allocator : node_allocators) {
    auto allocator = node_allocator.load();
    if (allocator != nullptr) {
      allocator->cache_.ClaimRemoteFreeList();
      allocator->cache_.FlushFreeListAll();
      delete (allocator);
    }
  }
}

void *SlabAllocatorRoot::operator new(size_t size) {
  kassert(size == sizeof(SlabAllocatorRoot));
  auto &allocator = slab_root_allocator->get_cpu_allocator();
  auto ret = allocator.Alloc();
  if (ret == nullptr) {
    throw std::bad_alloc();
  }
  return ret;
}

void SlabAllocatorRoot::operator delete(void *p) {
  auto &allocator = slab_root_allocator->get_cpu_allocator();
  allocator.Free(p);
}

size_t SlabAllocatorRoot::num_objects_per_slab() {
  return (PAGE_SIZE << order) / size;
}

SlabAllocator &SlabAllocatorRoot::get_cpu_allocator(size_t cpu_index) {
  auto allocator = cpu_allocators[cpu_index].get();
  if (allocator == nullptr) {
    allocator = new (cpus[cpu_index].nid) SlabAllocator(*this);
    cpu_allocators[cpu_index] = std::unique_ptr<SlabAllocator>(allocator);
  }
  return *allocator;
}

SlabAllocatorNode &SlabAllocatorRoot::get_node_allocator(nid_t nid) {
  size_t index = nid;
  auto allocator = node_allocators[index].load();
  if (allocator == nullptr) {
    auto new_allocator = new (nid) SlabAllocatorNode(*this, nid);
    if (!node_allocators[index]
             .compare_exchange_strong(allocator, new_allocator)) {
      delete allocator;
      allocator = node_allocators[index].load();
    } else {
      allocator = new_allocator;
    }
  }
  return *allocator;
}
