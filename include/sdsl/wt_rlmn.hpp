/* sdsl - succinct data structures library
    Copyright (C) 2011 Simon Gog

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see http://www.gnu.org/licenses/ .
*/
/*! \file wt_rlmn.hpp
    \brief wt_rlmn.hpp contains a class for a compressed wavelet tree.
	  Compression is achieved by exploiting runs in the input sequence.
    \author Simon Gog
*/
#ifndef INCLUDED_SDSL_WT_RLMN
#define INCLUDED_SDSL_WT_RLMN

#include "sdsl_concepts.hpp"
#include "int_vector.hpp"
#include "sd_vector.hpp"  // for standard initialisation of template parameters 
#include "util.hpp"
#include "wt_huff.hpp"
#include <algorithm> // for std::swap
#include <stdexcept>
#include <vector>
#include <utility> // for pair
#include <queue>
#include <iostream>

//! Namespace for the succinct data structure library.
namespace sdsl
{

//! A Wavelet Tree class for byte sequences.
/*!
 *    \par Space complexity
 *         \f$ nH_0 + 2|\Sigma|\log n + 2n + o(n) \f$ bits, where \f$n\f$
 *          is the size of the vector the wavelet tree was build for.
 *
 *   @ingroup wt
 *
 *  \tparam t_bitvector Type of the bitvector which is used to represent bf and
 *                      bl which mark the head of each run in the original
 *                      sequence.
 *  \tparam t_rank      Type of the rank support for bitvectors bf and bl.
 *  \tparam t_select    Type of the select support for bitvectors bf and lb.
 *  \tparam t_wt        Type of the wavelet tree for the string consisting of
 *                      the heads of the runs of the original sequence.
 *  \par Reference:
 *     Veli Mäkinen, Gonzalo Navarro:
 *     Succinct Suffix Arrays Based on Run-Length Encoding.
 *     CPM 2005: 45-56
 */
template<class t_bitvector = sd_vector<>,
         class t_rank = typename t_bitvector::rank_1_type,
         class t_select = typename t_bitvector::select_1_type,
         class t_wt = wt_huff<> >
class wt_rlmn
{
    public:
        typedef int_vector<>::size_type size_type;
        typedef unsigned char           value_type;
        typedef t_bitvector             bit_vector_type;
        typedef t_rank                  rank_support_type;
        typedef t_select                select_support_type;
        typedef t_wt                    wt_type;
        typedef wt_tag                  index_category;
        typedef byte_alphabet_tag       alphabet_category;
    private:
        size_type            m_size;      // size of the original input sequence
        bit_vector_type      m_bl;        // bit vector which indicates the starts of runs in
        // the BWT (or last column), i.e. _b_ _l_ast
        bit_vector_type      m_bf;        // bit vector which indicates the starts of runs in
        // the first column of the sorted suffixes, i.e _b_ _f_irst
        wt_type              m_wt;        // wavelet tree for all levels
        // two equal chars
        rank_support_type    m_bl_rank;   // rank support for bit vector bl
        rank_support_type    m_bf_rank;   // rank support for bit vector bf
        select_support_type  m_bl_select; // select support for bit vector bl
        select_support_type  m_bf_select; // select support for bit vector bf
        int_vector<64>       m_C;         //
        int_vector<64>       m_C_bf_rank; // stores the number of 1s in m_bf for the prefixes
        // m_bf[0..m_C[0]],m_bf[0..m_C[1]],....,m_bf[0..m_C[255]];
        // named C_s in the original paper

        void copy(const wt_rlmn& wt) {
            m_size          = wt.m_size;
            m_bl            = wt.m_bl;
            m_bf            = wt.m_bf;
            m_wt            = wt.m_wt;
            m_bl_rank       = wt.m_bl_rank;
            m_bl_rank.set_vector(&m_bl);
            m_bf_rank       = wt.m_bf_rank;
            m_bf_rank.set_vector(&m_bf);
            m_bl_select     = wt.m_bl_select;
            m_bl_select.set_vector(&m_bl);
            m_bf_select     = wt.m_bf_select;
            m_bf_select.set_vector(&m_bf);
            m_C             = wt.m_C;
            m_C_bf_rank     = wt.m_C_bf_rank;
        }

    public:
        const size_type& sigma;

        // Default constructor
        wt_rlmn():m_size(0), sigma(m_wt.sigma) {};

        //! Construct the wavelet tree from a file_buffer
        /*! \param text_buf  A int_vector_file_buffer to the original text.
         *  \param size      The length of the prefix of the text, for which the wavelet tree should be build.
         */
        // TODO: new signature: sdsl::file, size_type size
        wt_rlmn(int_vector_file_buffer<8>& text_buf, size_type size):m_size(size), sigma(m_wt.sigma) {
            std::string temp_file = text_buf.file_name + "_wt_rlmn_" + util::to_string(util::pid()) + "_" + util::to_string(util::id());
            osfstream wt_out(temp_file, std::ios::binary | std::ios::trunc);
            size_type bit_cnt=0;
            wt_out.write((char*)&bit_cnt, sizeof(bit_cnt)); // initial dummy write
            {
                // scope for bl and bf
                bit_vector bl = bit_vector(size, 0);
                m_C  = int_vector<64>(256, 0);
                text_buf.reset();
                uint8_t last_c = '\0';
                for (size_type i=0, r=0, r_sum=0; r_sum < size;) {
                    if (r_sum + r > size) {  // read not more than size chars in the next loop
                        r = size-r_sum;
                    }
                    for (; i < r+r_sum; ++i) {
                        uint8_t c = text_buf[i-r_sum];
                        if (last_c != c or i==0) {
                            bl[i] = 1;
                            wt_out.write((char*)&c, sizeof(c));
                            bit_cnt += 8;
                        }
                        ++m_C[c];
                        last_c = c;
                    }
                    r_sum += r;
                    r = text_buf.load_next_block();
                }

                wt_out.seekp(0, std::ios::beg);
                wt_out.write((char*)&bit_cnt, sizeof(bit_cnt));
                wt_out.close();

                for (size_type i=0, prefix_sum=0, t=0; i<256; ++i) {
                    t = m_C[i];
                    m_C[i] = prefix_sum;
                    prefix_sum += t;
                }

                int_vector<64> lf_map = m_C;
                bit_vector bf = bit_vector(size+1, 0);
                bf[size] = 1; // initialize last element
                text_buf.reset();
                for (size_type i=0, r=0, r_sum=0; r_sum < size;) {
                    if (r_sum + r > size) {  // read not more than size chars in the next loop
                        r = size-r_sum;
                    }
                    for (; i < r+r_sum; ++i) {
                        uint8_t c = text_buf[i-r_sum];
                        if (bl[i]) {
                            bf[lf_map[c]] = 1;
                        }
                        ++lf_map[c];
                    }
                    r_sum += r;
                    r = text_buf.load_next_block();
                }
                {
                    int_vector_file_buffer<8> temp_bwt_buf(temp_file);
                    util::assign(m_wt, wt_type(temp_bwt_buf, temp_bwt_buf.int_vector_size));
                    sdsl::remove(temp_file);
                }
                util::assign(m_bl, bl);
                util::assign(m_bf, bf);
            }

            util::init_support(m_bl_rank, &m_bl);
            util::init_support(m_bf_rank, &m_bf);
            util::init_support(m_bf_select, &m_bf);
            util::init_support(m_bl_select, &m_bl);
            m_C_bf_rank = int_vector<64>(256,0);
            for (size_type i=0; i<256; ++i) {
                m_C_bf_rank[i] = m_bf_rank(m_C[i]);
            }
        }

        //! Copy constructor
        wt_rlmn(const wt_rlmn& wt):sigma(wt.sigma) {
            copy(wt);
        }

        //! Assignment operator
        wt_rlmn& operator=(const wt_rlmn& wt) {
            if (this != &wt) {
                copy(wt);
            }
            return *this;
        }

        //! Swap operator
        void swap(wt_rlmn& wt) {
            if (this != &wt) {
                std::swap(m_size, wt.m_size);
                m_bl.swap(wt.m_bl);
                m_bf.swap(wt.m_bf);
                m_wt.swap(wt.m_wt);

                m_bl_rank.swap(wt.m_bl_rank);
                m_bl_rank.set_vector(&m_bl);
                wt.m_bl_rank.set_vector(&(wt.m_bl));
                m_bf_rank.swap(wt.m_bf_rank);
                m_bf_rank.set_vector(&m_bf);
                wt.m_bf_rank.set_vector(&(wt.m_bf));

                m_bl_select.swap(wt.m_bl_select);
                m_bl_select.set_vector(&m_bl);
                wt.m_bl_select.set_vector(&(wt.m_bl));
                m_bf_select.swap(wt.m_bf_select);
                m_bf_select.set_vector(&m_bf);
                wt.m_bf_select.set_vector(&(wt.m_bf));

                m_C.swap(wt.m_C);
                m_C_bf_rank.swap(wt.m_C_bf_rank);
            }
        }

        //! Returns the size of the original vector.
        size_type size()const {
            return m_size;
        }

        //! Returns whether the wavelet tree contains no data.
        bool empty()const {
            return 0 == m_size;
        }

        //! Recovers the i-th symbol of the original vector.
        /*! \param i The index of the symbol in the original vector. \f$i \in [0..size()-1]\f$
         *    \return The i-th symbol of the original vector.
         *  \par Time complexity
         *        \f$ \Order{H_0} \f$ on average, where \f$ H_0 \f$ is the zero order entropy of
         *      the sequence
         */
        value_type operator[](size_type i)const {
            assert(i < size());
            return m_wt[m_bl_rank(i+1)-1];
        };

        //! Calculates how many symbols c are in the prefix [0..i-1] of the supported vector.
        /*!
         *  \param i The exclusive index of the prefix range [0..i-1], so \f$i\in[0..size()]\f$.
         *  \param c The symbol to count the occurrences in the prefix.
         *  \return The number of occurrences of symbol c in the prefix [0..i-1] of the supported vector.
         *  \par Time complexity
         *        \f$ \Order{H_0} \f$ on average, where \f$ H_0 \f$ is the zero order entropy of
         *      the sequence
         */
        size_type rank(size_type i, value_type c)const {
            assert(i <= size());
            if (i == 0)
                return 0;
            size_type wt_ex_pos = m_bl_rank(i);
            size_type c_runs = m_wt.rank(wt_ex_pos, c);
            if (c_runs == 0)
                return 0;
            if (m_wt[wt_ex_pos-1] == c) {
                size_type c_run_begin = m_bl_select(wt_ex_pos);
                return m_bf_select(m_C_bf_rank[c] + c_runs) - m_C[c] + i - c_run_begin;
            } else {
                return m_bf_select(m_C_bf_rank[c] + c_runs + 1) - m_C[c];
            }
        };

        //! Calculates how many occurrences of symbol wt[i] are in the prefix [0..i-1] of the supported sequence.
        /*!
         *  \param i The index of the symbol.
         *  \param c Reference that will contain the symbol at position i after the execution of the method.
         *  \return The number of occurrences of symbol wt[i] in the prefix [0..i-1]
         *    \par Time complexity
         *        \f$ \Order{H_0} \f$
         */
        size_type inverse_select(size_type i, value_type& c)const {
            assert(i < size());
            if (i == 0) {
                c = m_wt[0];
                return 0;
            }
            size_type wt_ex_pos = m_bl_rank(i+1);
            size_type c_runs = m_wt.inverse_select(wt_ex_pos-1, c)+1;
            if (c_runs == 0)
                return 0;
            if (m_wt[wt_ex_pos-1] == c) {
                size_type c_run_begin = m_bl_select(wt_ex_pos);
                return m_bf_select(m_C_bf_rank[c] + c_runs) - m_C[c] + i - c_run_begin;
            } else {
                return m_bf_select(m_C_bf_rank[c] + c_runs + 1) - m_C[c];
            }
        }

        //! Calculates the ith occurrence of the symbol c in the supported vector.
        /*!
         *  \param i The ith occurrence. \f$i\in [1..rank(size(),c)]\f$.
         *  \param c The symbol c.
         *  \par Time complexity
         *       \f$ \Order{H_0} \f$ on average, where \f$ H_0 \f$ is the zero order
         *        entropy of the sequence
         */
        size_type select(size_type i, value_type c)const {
            assert(i > 0);
            assert(i <= rank(size(), c));
            size_type c_runs = m_bf_rank(m_C[c]+i) - m_C_bf_rank[c];
            size_type offset = m_C[c] + i - 1 - m_bf_select(c_runs + m_C_bf_rank[c]);
            return m_bl_select(m_wt.select(c_runs, c)+1) + offset;
        };

        //! Serializes the data structure into the given ostream
        size_type serialize(std::ostream& out, structure_tree_node* v=NULL, std::string name="")const {
            structure_tree_node* child = structure_tree::add_child(v, name, util::class_name(*this));
            size_type written_bytes = 0;
            written_bytes += write_member(m_size, out, child, "size");
            written_bytes += m_bl.serialize(out, child, "bl");
            written_bytes += m_bf.serialize(out, child, "bf");
            written_bytes += m_wt.serialize(out, child, "wt");
            written_bytes += m_bl_rank.serialize(out, child, "bl_rank");
            written_bytes += m_bf_rank.serialize(out, child, "bf_rank");
            written_bytes += m_bl_select.serialize(out, child, "bl_select");
            written_bytes += m_bf_select.serialize(out, child, "bf_select");
            written_bytes += m_C.serialize(out, child, "C");
            written_bytes += m_C_bf_rank.serialize(out, child, "C_bf_rank");
            structure_tree::add_size(child, written_bytes);
            return written_bytes;
        }

        //! Loads the data structure from the given istream.
        void load(std::istream& in) {
            read_member(m_size, in);
            m_bl.load(in);
            m_bf.load(in);
            m_wt.load(in);
            m_bl_rank.load(in, &m_bl);
            m_bf_rank.load(in, &m_bf);
            m_bl_select.load(in, &m_bl);
            m_bf_select.load(in, &m_bf);
            m_C.load(in);
            m_C_bf_rank.load(in);
        }
};


}// end namespace sdsl

#endif // end file 
