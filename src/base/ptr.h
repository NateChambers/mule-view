#ifndef __BASE_PTR__
#define __BASE_PTR__

template<class T, bool is_array = false>
class LocalPtr
{
  mutable T* m_ptr;
public:
  LocalPtr()
    : m_ptr(NULL)
  {}
  LocalPtr(T* value)
    : m_ptr(value)
  {}
  LocalPtr(LocalPtr<T> const& value)
    : m_ptr(value.m_ptr)
  {
    value.m_ptr = NULL;
  }
  ~LocalPtr()
  {
    if (is_array)
      delete[] m_ptr;
    else
      delete m_ptr;
  }

  T* operator ->()
  {
    return m_ptr;
  }
  operator T*()
  {
    return m_ptr;
  }
  T* ptr()
  {
    return m_ptr;
  }
  T& operator[] (int i)
  {
    return m_ptr[i];
  }

  T const* operator ->() const
  {
    return m_ptr;
  }
  operator T const*() const
  {
    return m_ptr;
  }
  T const* ptr() const
  {
    return m_ptr;
  }
  T const& operator[] (int i) const
  {
    return m_ptr[i];
  }

  LocalPtr<T>& operator = (T* value)
  {
    if (m_ptr != value)
    {
      delete m_ptr;
      m_ptr = value;
    }
    return *this;
  }
  template <class To>
  LocalPtr<T>& operator = (LocalPtr<To>& value)
  {
    delete m_ptr;
    m_ptr = value.m_ptr;
    value.m_ptr = NULL;
    return *this;
  }

  T* yield()
  {
    T* val = m_ptr;
    m_ptr = NULL;
    return val;
  }
};

template<class T>
class RefPtr
{
  T* m_ptr;
public:
  RefPtr()
    : m_ptr(NULL)
  {}
  RefPtr(T* value)
    : m_ptr(value)
  {}
  RefPtr(RefPtr<T> const& value)
    : m_ptr(value.m_ptr->addref())
  {}
  ~RefPtr()
  {
    if (m_ptr)
      m_ptr->release();
  }

  T* operator ->()
  {
    return m_ptr;
  }
  operator T*()
  {
    return m_ptr;
  }
  T* ptr()
  {
    return m_ptr;
  }

  T const* operator ->() const
  {
    return m_ptr;
  }
  operator T const*() const
  {
    return m_ptr;
  }
  T const* ptr() const
  {
    return m_ptr;
  }

  RefPtr<T>& operator = (T* value)
  {
    if (m_ptr != value)
    {
      m_ptr->release();
      m_ptr = value;
    }
    return *this;
  }
};

#endif // __BASE_PTR__
