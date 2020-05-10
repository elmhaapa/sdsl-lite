
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
class hac_vector {
  public:
    typedef uint8_t size_type;
    typedef uint64_t value_type;

  private:
    size_type* m_data;
    value_type m_size = 0;
    // Treshold value after which we put in map.
    value_type treshold = (1 << 8) - 2;
    // Over the treshold token
    value_type ott = treshold + 1;
    std::unordered_map<value_type, value_type> m_map;
    void copy(const hac_vector& v)
    {
      m_data = v.m_data;
      m_map = v.m_map;
      m_size = v.m_size;
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
      return m_size;
    }
    static size_type max_size()
    {
      return int_vector<>::max_size();
    }
    bool empty() const
    {
      return m_size == 0;
    }

    value_type operator[](value_type i)const
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

template<class Container>
hac_vector::hac_vector(const Container& c)
{
  value_type n = c.size(), val = 0;
  m_data = new size_type[n];
  m_size = n;
  for (value_type i = 0; i < n; ++i) {
    val = c[i];
    if (val <= treshold) {
      m_data[i] = val;
    } else {
      m_map[i] = val;
      m_data[i] = ott;
    }
  }

}

template<uint8_t int_width>
hac_vector::hac_vector(int_vector_buffer<int_width>& v_buf)
{
  value_type n = v_buf.size();
  value_type val = 0;
  m_data = new size_type[n];
  m_size = n;
  for (value_type i = 0; i < n; ++i) {
    val = v_buf[i];
    if (val <= treshold) {
      m_data[i] = val;
    } else {
      m_map[i] = val;
      m_data[i] = ott;
    }
  }
}

void hac_vector::load(std::istream& in)
{
  value_type val = 0;
  dac_vector<> tmpv;
  sdsl::load(tmpv, in);
  m_data = new size_type[tmpv.size()];
  m_size = tmpv.size();
  for (value_type i = 0; i < tmpv.size(); ++i) {
    val = tmpv[i];
    if (val <= treshold) {
      m_data[i] = val;
    } else {
      m_map[i] = val;
      m_data[i] = ott;
    }
  }
}

typename hac_vector::size_type hac_vector::serialize(std::ostream& out, structure_tree_node* v, std::string name)const
{
  value_type val = 0;
  value_type n = m_size;
  // FIXME this is not the best way to go about it.
  int_vector<64> tmpv = int_vector<64>(n, 0);
  for (value_type i = 0; i < n; ++i) {
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

