
#ifndef SDSL_HAC_VECTOR
#define SDSL_HAC_VECTOR

#include "int_vector.hpp"
#include <unordered_map>

namespace sdsl
{
/*
 * Integer vector class to tease out the overhead cost associated with dac_vector.
 * The 64 bit integers are saved in two structures: sdsl::int_vector<w_i> & std::unordered_map.
 * Parametere w_i is the integer width: 8, 16 or 32 bits.
 * If integer is less than or equal treshold value 2^w_i - 2 we save it in the int_vector<w_i>,
 * otherwise we store it in unordered_map<w_i, uint64_t>.
 * */
template<uint8_t i_w>
class hac_vector {
  public:
    typedef typename int_vector<i_w>::size_type size_type;
    typedef typename int_vector<>::value_type value_type;

  private:
    int_vector<i_w> m_data;
    // Treshold value after which we put in map.
    value_type treshold = (1 << i_w) - 2;
    // Over the treshold token
    value_type ott = treshold + 1;
    std::unordered_map<size_type, value_type> m_map;
    void copy(const hac_vector& v)
    {
      m_data = v.m_data;
      m_map = v.m_map;
    }
  public:
    hac_vector() = default;
    hac_vector(const hac_vector& v)
    {
      copy(v);
    }

    hac_vector(hac_vector&& v)
    {
      *this = std::move(v);
    }
    hac_vector& operator=(const hac_vector& v)
    {
      if (this != &v) {
        copy(v);
      }
      return *this;
    }

    // Constructor for container of unsigned integers.
    template<class Container>
    hac_vector(const Container& C);

    // Constructor for an int_vector_buffer of unsigned integers.
    template<uint8_t int_width>
    hac_vector(int_vector_buffer<int_width>& v_buf);

    size_type size() const
    {
      return m_data.size();
    }
    static size_type max_size()
    {
      return int_vector<>::max_size();
    }
    bool empty() const
    {
      return m_data.empty();
    }

    value_type operator[](size_type i)const
    {
      if (m_data[i] <= treshold) {
        // access array
        return m_data[i];
      } else {
        int val = 0;
        auto it = m_map.find(i);
        if (it != m_map.end()) {
          val = it->second;
        }
        return val;
      }
    }
  size_type serialize(std::ostream& out, structure_tree_node* v=nullptr, std::string name = "") const;

  void load(std::istream& in);

};

template<uint8_t i_w>
template<class Container>
hac_vector<i_w>::hac_vector(const Container& c)
{
  size_type n = c.size(), val = 0;
  m_data = int_vector<i_w>(n, ott);
  for (size_type i = 0; i < n; ++i) {
    val = c[i];
    if (val <= treshold) {
      m_data[i] = val;
    } else {
      m_map[i] = val;
    }
  }
}

template<uint8_t i_w>
template<uint8_t int_width>
hac_vector<i_w>::hac_vector(int_vector_buffer<int_width>& v_buf)
{
  size_type n = v_buf.size(), val = 0;
  m_data = int_vector<i_w>(n, ott);
  for (size_type i = 0; i < n; ++i) {
    val = v_buf[i];
    if (val <= treshold) {
      m_data[i] = val;
    } else {
      m_map[i] = val;
    }
  }
}

template<uint8_t i_w>
void hac_vector<i_w>::load(std::istream& in)
{
  value_type val = 0;
  dac_vector<> tmpv;
  sdsl::load(tmpv, in);
  m_data.resize(tmpv.size());
  for (size_type i = 0; i < tmpv.size(); ++i) {
    val = tmpv[i];
    if (val <= treshold) {
      m_data[i] = val;
    } else {
      m_map[i] = val;
      m_data[i] = ott;
    }
  }
}

template<uint8_t i_w>
typename hac_vector<i_w>::size_type hac_vector<i_w>::serialize(std::ostream& out, structure_tree_node* v, std::string name)const
{
  value_type val = 0;
  size_type n = m_data.size();
  // FIXME this is not the best way to go about it.
  int_vector<64> tmpv = int_vector<64>(n, 0);
  for (size_type i = 0; i < n; ++i) {
    val = m_data[i];
    if (val > treshold) {
      auto it = m_map.find(i);
      if (it != m_map.end()) {
        val = it->second;
      }
    }
   tmpv[i] = val;
  }
  dac_vector<> dv(tmpv);
  return dv.serialize(out, v, name);
}

} 
#endif

