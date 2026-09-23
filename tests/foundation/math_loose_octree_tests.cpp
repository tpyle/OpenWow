#include "openwow/foundation/math/loose_octree.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <vector>

using Catch::Approx;
using openwow::math::CLooseOctree;
using openwow::math::LooseOctreeAABB;
using openwow::math::LooseOctreeNode;

namespace {

LooseOctreeNode MakeNode(float min_x, float min_y, float min_z, float max_x, float max_y,
                         float max_z) {
  LooseOctreeNode node{};
  std::memset(&node, 0, sizeof(node));
  node.bounds = {min_x, max_x, min_y, max_y, min_z, max_z};
  return node;
}

} // namespace

TEST_CASE("CLooseOctree::MergeAABB produces the componentwise union of two boxes",
          "[foundation][loose_octree]") {
  const LooseOctreeAABB a{0.0f, 10.0f, -5.0f, 5.0f, 1.0f, 2.0f};
  const LooseOctreeAABB b{-3.0f, 4.0f, 0.0f, 20.0f, -1.0f, 3.0f};

  LooseOctreeAABB merged{};
  CLooseOctree::MergeAABB(a, b, merged);

  CHECK(merged.minX == Approx(-3.0f));
  CHECK(merged.maxX == Approx(10.0f));
  CHECK(merged.minY == Approx(-5.0f));
  CHECK(merged.maxY == Approx(20.0f));
  CHECK(merged.minZ == Approx(-1.0f));
  CHECK(merged.maxZ == Approx(3.0f));
}

TEST_CASE("CLooseOctree::ComputeTreeBounds on an empty tree returns an all-zero box",
          "[foundation][loose_octree]") {
  CLooseOctree tree;
  LooseOctreeAABB bounds{};
  tree.ComputeTreeBounds(bounds);
  CHECK(bounds.minX == 0.0f);
  CHECK(bounds.maxX == 0.0f);
  CHECK(bounds.minY == 0.0f);
  CHECK(bounds.maxY == 0.0f);
  CHECK(bounds.minZ == 0.0f);
  CHECK(bounds.maxZ == 0.0f);
}

TEST_CASE("CLooseOctree::InsertNode on a fresh tree makes the node the root and sets kInTree",
          "[foundation][loose_octree]") {
  CLooseOctree tree;
  LooseOctreeNode node = MakeNode(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

  tree.InsertNode(&node);

  CHECK(tree.Root() == &node);
  CHECK(node.IsInTree());

  LooseOctreeAABB bounds{};
  tree.ComputeTreeBounds(bounds);
  CHECK(bounds.minX == Approx(0.0f));
  CHECK(bounds.maxX == Approx(1.0f));
}

TEST_CASE("CLooseOctree::InsertNode on an already-inserted node is a no-op",
          "[foundation][loose_octree]") {
  CLooseOctree tree;
  LooseOctreeNode node = MakeNode(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  tree.InsertNode(&node);
  const std::uint32_t flags_after_first_insert = node.flags;

  tree.InsertNode(&node); // node already has kInTree set
  CHECK(node.flags == flags_after_first_insert);
  CHECK(tree.Root() == &node);
}

TEST_CASE("CLooseOctree::RemoveNode on the sole root node clears the tree",
          "[foundation][loose_octree]") {
  CLooseOctree tree;
  LooseOctreeNode node = MakeNode(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  tree.InsertNode(&node);

  tree.RemoveNode(&node);
  CHECK(tree.Root() == nullptr);
  CHECK_FALSE(node.IsInTree());
}

TEST_CASE("CLooseOctree::RemoveNode on a node not in the tree is a no-op",
          "[foundation][loose_octree]") {
  CLooseOctree tree;
  LooseOctreeNode node = MakeNode(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  // Never inserted -- flags is all zero, kInTree not set.
  tree.RemoveNode(&node);
  CHECK(tree.Root() == nullptr);
  CHECK_FALSE(node.IsInTree());
}

TEST_CASE("CLooseOctree: a node can be removed and then reinserted as the sole occupant",
          "[foundation][loose_octree]") {
  CLooseOctree tree;
  LooseOctreeNode node = MakeNode(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  tree.InsertNode(&node);
  tree.RemoveNode(&node);

  tree.InsertNode(&node);
  CHECK(tree.Root() == &node);
  CHECK(node.IsInTree());
}

TEST_CASE("CLooseOctree::UpdateNode on a node not yet in the tree delegates to InsertNode",
          "[foundation][loose_octree]") {
  CLooseOctree tree;
  LooseOctreeNode node = MakeNode(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  tree.UpdateNode(&node);
  CHECK(tree.Root() == &node);
  CHECK(node.IsInTree());
}

TEST_CASE("CLooseOctree: two nodes occupying the exact same quantized cell chain together "
          "via InsertLeafChain without needing a free branch node",
          "[foundation][loose_octree]") {
  // Identical bounds -> identical quantized (level, qx, qy, qz), so
  // InsertInternal's split-point search finds no differing bit and takes
  // the no-split "chain onto this leaf" path instead of ever calling
  // AllocBranch(). This is deliberately the *only* multi-node scenario
  // exercised with confidence here -- see the test below for why a
  // genuinely spatially-separated second node is a much riskier case on a
  // fresh tree.
  CLooseOctree tree;
  LooseOctreeNode node_a = MakeNode(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  LooseOctreeNode node_b = MakeNode(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

  tree.InsertNode(&node_a);
  tree.InsertNode(&node_b);

  CHECK(node_a.IsInTree());
  CHECK(node_b.IsInTree());
  // One of the two must be reachable from the other via the `next` chain
  // (InsertLeafChain always links through `next`, never `left`/`right`,
  // for two same-cell leaves).
  const bool chained = (tree.Root() == &node_a && node_a.next == &node_b) ||
                       (tree.Root() == &node_b && node_b.next == &node_a);
  CHECK(chained);
}

TEST_CASE("CLooseOctree: FIXED -- inserting a second, spatially-separated node into a "
          "fresh tree correctly splits and attaches it",
          "[foundation][loose_octree]") {
  // This pins the fix for what was the most significant finding from
  // testing this class: a brand-new CLooseOctree's internal branch-node
  // free list (freeList_) started out empty with no public API to seed
  // it, so AllocBranch() returned nullptr and InsertInternal() silently
  // failed to attach any second, spatially-distinct node -- it reported
  // IsInTree() == true while being unreachable from Root(), a "ghost"
  // node. AllocBranch() now falls back to allocating (and the tree
  // owning) a fresh branch node when the free list is empty, so a real
  // split succeeds instead. This test used to assert the opposite
  // (node_b unreachable); flipped after the fix, per the same tree/node
  // setup so the fix is exercised under the exact conditions that
  // exposed the bug.
  //
  // invSize_ is left at its default (1.0) in every other test in this
  // file, which only makes sense for objects/coordinates on the order of
  // magnitude of 1 -- it represents 1/worldSize, and quantization
  // multiplies coordinates by invSize_ * kQuantScale (~2^30) before
  // truncating to uint32_t. A coordinate of 1000 at invSize_ == 1.0 would
  // overflow that truncation (undefined behavior, not just a wrapped
  // value), so this test explicitly sets a small invSize_ (1/256) to keep
  // both nodes' quantized coordinates in range while still being far
  // enough apart, relative to their own size, to force a real tree split.
  CLooseOctree tree;
  tree.SetInvSize(1.0f / 256.0f);
  LooseOctreeNode node_a = MakeNode(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  LooseOctreeNode node_b = MakeNode(100.0f, 100.0f, 100.0f, 101.0f, 101.0f, 101.0f);

  tree.InsertNode(&node_a);
  tree.InsertNode(&node_b);

  CHECK(node_a.IsInTree());
  CHECK(node_b.IsInTree());

  // Walk everything reachable from Root() via next/left/right and confirm
  // node_b actually ended up in it (both directly, and via a real branch
  // split rather than a next-chain, since node_a/node_b are in different
  // quantized cells here).
  bool node_a_reachable = false;
  bool node_b_reachable = false;
  bool split_branch_seen = false;
  LooseOctreeNode *stack[64];
  int stack_size = 0;
  if (tree.Root())
    stack[stack_size++] = tree.Root();
  while (stack_size > 0) {
    LooseOctreeNode *current = stack[--stack_size];
    if (current == &node_a)
      node_a_reachable = true;
    if (current == &node_b)
      node_b_reachable = true;
    if (current->left || current->right)
      split_branch_seen = true;
    if (current->left)
      stack[stack_size++] = current->left;
    if (current->right)
      stack[stack_size++] = current->right;
    if (current->next)
      stack[stack_size++] = current->next;
  }

  CHECK(node_a_reachable);
  CHECK(node_b_reachable);
  CHECK(split_branch_seen);

  LooseOctreeAABB bounds{};
  tree.ComputeTreeBounds(bounds);
  CHECK(bounds.minX == Approx(0.0f));
  CHECK(bounds.maxX == Approx(101.0f));
}

TEST_CASE("CLooseOctree: a tree can hold many spatially-separated nodes, each independently "
          "removable",
          "[foundation][loose_octree]") {
  CLooseOctree tree;
  tree.SetInvSize(1.0f / 256.0f);

  std::vector<LooseOctreeNode> nodes;
  nodes.reserve(20);
  for (int i = 0; i < 20; ++i) {
    const float base = static_cast<float>(i) * 5.0f;
    nodes.push_back(MakeNode(base, base, base, base + 1.0f, base + 1.0f, base + 1.0f));
  }
  for (auto &node : nodes) {
    tree.InsertNode(&node);
  }
  for (const auto &node : nodes) {
    CHECK(node.IsInTree());
  }

  // Every node must be reachable from Root(), not just flagged in-tree.
  int reachable_count = 0;
  LooseOctreeNode *stack[128];
  int stack_size = 0;
  if (tree.Root())
    stack[stack_size++] = tree.Root();
  while (stack_size > 0) {
    LooseOctreeNode *current = stack[--stack_size];
    for (const auto &node : nodes) {
      if (current == &node) {
        ++reachable_count;
        break;
      }
    }
    if (current->left)
      stack[stack_size++] = current->left;
    if (current->right)
      stack[stack_size++] = current->right;
    if (current->next)
      stack[stack_size++] = current->next;
  }
  CHECK(reachable_count == 20);

  // Removing half of them should not disturb the other half.
  for (int i = 0; i < 20; i += 2) {
    tree.RemoveNode(&nodes[static_cast<std::size_t>(i)]);
  }
  for (int i = 0; i < 20; ++i) {
    CAPTURE(i);
    CHECK(nodes[static_cast<std::size_t>(i)].IsInTree() == (i % 2 != 0));
  }
}
