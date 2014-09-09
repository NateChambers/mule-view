#ifndef __ACTREE__
#define __ACTREE__

#include "base/types.h"
#include "pool.h"

namespace ac
{

namespace traits
{
  class Alpha
  {
  public:
    enum {size = 27};
    static int get(char c)
    {
      if (c == ' ') return 0;
      if (c >= 'a' && c <= 'z') return c - 'a' + 1;
      if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
      return -1;
    }
  };
}

template <class Value, class Tr = traits::Alpha>
class Tree
{
  FixedMemoryPool pool;
  struct Node
  {
    Node* parent;
    Node* next[Tr::size];
    int chr;
    Node* link;
    Value val;
    int len;
  };
  Node* root;
public:
  Tree()
    : pool(sizeof(Node))
  {
    root = (Node*) pool.alloc();
    memset(root, 0, sizeof(Node));
  }

  void add(char const* str, Value val)
  {
    int len;
    Node* node = root;
    for (len = 0; str[len]; len++)
    {
      int chr = Tr::get(str[len]);
      if (chr < 0) continue;
      if (node->next[chr] == NULL)
      {
        Node* temp = (Node*) pool.alloc();
        memset(temp, 0, sizeof(Node));
        temp->parent = node;
        temp->chr = chr;
        node->next[chr] = temp;
      }
      node = node->next[chr];
    }
    node->val = val;
    node->len = len;
  }
  void build()
  {
    Node* head = root;
    Node* tail = root;
    while (head)
    {
      Node* link;
      if (!head->parent || head->parent == root)
        link = root;
      else
        link = head->parent->link->next[head->chr];
      if (link->len > head->len)
      {
        head->val = link->val;
        head->len = link->len;
      }
      for (int i = 0; i < Tr::size; i++)
      {
        if (!head->next[i])
        {
          if (head->parent)
            head->next[i] = link->next[i];
          else
            head->next[i] = root;
        }
        else
        {
          tail->link = head->next[i];
          tail = tail->link;
        }
      }
      Node* temp = head->link;
      head->link = link;
      head = temp;
    }
  }

  Value get(char const* str)
  {
    Value bestval = 0;
    int bestlen = 0;
    Node* node = root;
    for (int i = 0; str[i]; i++)
    {
      int chr = Tr::get(str[i]);
      if (chr < 0) continue;
      node = node->next[chr];
      if (node->len > bestlen)
      {
        bestval = node->val;
        bestlen = node->len;
      }
    }
    return bestval;
  }
};

}

#endif // __ACTREE__
