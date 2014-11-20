#ifndef __gamgee_merged_vcf_lut__
#define __gamgee_merged_vcf_lut__

#include<assert.h>

#include "htslib/vcf.h"
#include <vector>
#include "../variant_header.h"
#include "../missing.h"


//forward declaration for friend function of MergedVCFLUTBase
template<bool inputs_2_merged_LUT_is_input_ordered, bool merged_2_inputs_LUT_is_input_ordered>
void test_lut_base();

namespace gamgee
{
  //forward declaration of friend class of MergedVCFLUTBase
  template<bool fields_forward_LUT_ordering, bool fields_reverse_LUT_ordering, bool samples_forward_LUT_ordering, bool samples_reverse_LUT_ordering>
  class VariantHeaderMerger;

  namespace utils
  {
    /**
     * LUT = Look Up Table (to avoid confusion with map, unordered_map etc)
     * @brief Base class to store look up information between fields of merged header and input headers
     * @note This is the helper class for VariantHeaderMerger to store mapping for fields and samples
     * Each MergedVCFLUTBase object contains 2 matrices (vector of vector): one for mapping input field idx to merged field idx (m_inputs_2_merged_lut)
     * and the second for mapping merged field idx to input field idx (m_merged_2_inputs_lut).
     *
     * Missing field information is stored as bcf_int32_missing, but should be checked with gamgee::missing() function
     * 
     * The boolean template parameters specify how the 2 tables are laid out in memory - whether the outer vector corresponds to fields or input vcfs.
     * For example, in object of type MergedVCFLUTBase<true, true>, both LUTs are laid out such that m_inputs_2_merged_lut[0] contains mappings 
     * for all fields for input VCF file 0. This would lead to fast traversal of all fields for a given input VCF (cache locality). 
     * However, traversing over all input VCFs for a given field would be slow (many cache misses).
     * The object MergedVCFLUTBase<false,false> would have the exact opposite behavior
     * 
     * The 'best' value of the template parameters depends on the application using the LUT.
     * Almost all the 'complexity' of the code comes from being able to handle the different layouts in a transparent manner
     *
     * Alternate explanation:
     * This class contains two matrices (vector<vector<int>>) to store the mapping information:
     * m_inputs_2_merged_lut and m_merged_2_inputs_lut. You can layout each matrix in one of the 2 following ways:
     * (a) LUT[i][j]  corresponds to input VCF i and field j 
     * (b) LUT[i][j]  corresponds to field i and input VCF j
     * Option (a) is optimal where you are looking at all the fields of a VCF in quick succession,
     * while (b) is optimal when you are looking at all VCFs for a particular field.
     * The 2 boolean template parameters control the layout of the two matrices. If the parameter value is true,
     * then option (a) is picked, else option (b)
     *
     * Although the class provides functions to resize the tables, for obtaining good performance, reallocations should be extremely
     * infrequent. Making the resize_luts_if_needed() a protected member forces developers to think twice instead of blindly calling this function.
     *
     * Uses the enable_if trick from http://en.cppreference.com/w/cpp/types/enable_if  (foo3) to handle different memory layouts
     * 
     **/
    template<bool inputs_2_merged_LUT_is_input_ordered, bool merged_2_inputs_LUT_is_input_ordered>
    class MergedVCFLUTBase
    {
      template<bool fields_forward_LUT_ordering, bool fields_reverse_LUT_ordering, bool samples_forward_LUT_ordering, bool samples_reverse_LUT_ordering>
      friend class gamgee::VariantHeaderMerger;
      template<bool v1, bool v2>
      friend void ::test_lut_base();
      public:
      /**
       * @brief: clear all mappings
       */
      inline void reset_luts()
      {
	for(auto& vec : m_inputs_2_merged_lut)
	  reset_vector(vec);
	for(auto& vec : m_merged_2_inputs_lut)
	  reset_vector(vec);
      }

      /*
       * @brief Add a valid mapping between input VCF and merged VCF
       * @note all parameters should be valid parameters, no bcf_int32_missing allowed, use reset_() functions to invalidate existing mapping
       * @param inputGVCFIdx index of the input VCF file
       * @param inputIdx index of the field in the input VCF file - field could be anything header field,sample,allele etc
       * @param mergedIdx index of the field in the merged VCF file
       */
      inline void add_input_merged_idx_pair(unsigned inputGVCFIdx, int inputIdx, int mergedIdx)
      {
	set_merged_idx_for_input(inputGVCFIdx, inputIdx, mergedIdx);
	set_input_idx_for_merged(inputGVCFIdx, inputIdx, mergedIdx);
      }

      /**
       * @brief Get field idx for input VCF inputGVCFIdx corresponding to field idx mergedIdx in the mergedVCF file
       * @note Uses the enable_if trick from http://en.cppreference.com/w/cpp/types/enable_if  (foo3) to handle different memory layouts
       * The enable_if<M> corresponds to the case where merged_2_inputs_LUT_is_input_ordered = true, hence, the rows correspond to input VCFs
       * The enable_if<!M> corresponds to the case where merged_2_inputs_LUT_is_input_ordered = false, hence, the rows correspond to fields
       * @param inputGVCFIdx index of the input VCF file
       * @param mergedIdx index of the field in the merged VCF file
       * @return index of the field in the input VCF file
       */
      template <bool M = merged_2_inputs_LUT_is_input_ordered, typename std::enable_if<M>::type* = nullptr>
      inline int get_input_idx_for_merged(unsigned inputGVCFIdx, int mergedIdx) const
      { return get_lut_value(m_merged_2_inputs_lut, inputGVCFIdx, mergedIdx); }
      template <bool M = merged_2_inputs_LUT_is_input_ordered, typename std::enable_if<!M>::type* = nullptr>
      inline int get_input_idx_for_merged(unsigned inputGVCFIdx, int mergedIdx) const
      { return get_lut_value(m_merged_2_inputs_lut, mergedIdx, inputGVCFIdx); }

      /**
       * @brief Get field idx for the merged VCF corresponding to field idx inputIdx in the input VCF of index inputGVCFIdx
       * @note Uses the enable_if trick from http://en.cppreference.com/w/cpp/types/enable_if  (foo3) to handle different memory layouts
       * The enable_if<M> corresponds to the case where inputs_2_merged_LUT_is_input_ordered = true, hence, the rows correspond to input VCFs
       * The enable_if<!M> corresponds to the case where inputs_2_merged_LUT_is_input_ordered = false, hence, the rows correspond to fields
       * @param inputGVCFIdx index of the input VCF file
       * @param inputIdx index of the field in the input VCF file
       * @return index of the field in the merged VCF file
       */
      template <bool M = inputs_2_merged_LUT_is_input_ordered, typename std::enable_if<M>::type* = nullptr>
      inline int get_merged_idx_for_input(unsigned inputGVCFIdx, int inputIdx) const
      { return get_lut_value(m_inputs_2_merged_lut, inputGVCFIdx, inputIdx); }
      template <bool M = inputs_2_merged_LUT_is_input_ordered, typename std::enable_if<!M>::type* = nullptr>
      inline int get_merged_idx_for_input(unsigned inputGVCFIdx, int inputIdx) const
      { return get_lut_value(m_inputs_2_merged_lut, inputIdx, inputGVCFIdx); }

      /**
       * @brief reset/invalidate merged field index for field inputIdx of input VCF inputGVCFIdx
       * @param inputGVCFIdx index of the input VCF file
       * @param inputIdx index of the field in the input VCF file
       */
      inline void reset_merged_idx_for_input(unsigned inputGVCFIdx, int inputIdx)
      {	set_merged_idx_for_input(inputGVCFIdx, inputIdx, gamgee::missing_values::int32); }
      /**
       * @brief reset/invalidate the input field index for input VCF inputGVCFIdx for merged field mergedIdx
       * @param inputGVCFIdx index of the input VCF file
       * @param mergedIdx index of the field in the merged VCF file
       */
      inline void reset_input_idx_for_merged(unsigned inputGVCFIdx, int mergedIdx)
      {	set_input_idx_for_merged(inputGVCFIdx, gamgee::missing_values::int32, mergedIdx); }

      protected:
      //Only inherited classes should call constructor,destructor etc
      MergedVCFLUTBase(); 
      MergedVCFLUTBase(unsigned numInputGVCFs, unsigned numMergedFields);
      ~MergedVCFLUTBase() = default;
      /**
       * @brief deallocates memory
       */
      void clear();

      unsigned m_num_input_vcfs;
      unsigned m_num_merged_fields;

      /**
       *  @brief resize LUT functions 
       *  @note should be called relatively infrequently (more precisely, the reallocation code inside these resize functions should be called
       *  infrequently
       *  @param numInputGVCFs number of input VCFs
       *  @param numMergedFields number of fields combined across all input VCFs
       */
      template <bool M = inputs_2_merged_LUT_is_input_ordered, typename std::enable_if<M>::type* = nullptr>
      void resize_inputs_2_merged_lut_if_needed(unsigned numInputGVCFs, unsigned numMergedFields)
      {	resize_and_reset_lut(m_inputs_2_merged_lut, numInputGVCFs, numMergedFields, m_num_input_vcfs, m_num_merged_fields); }

      template <bool M = inputs_2_merged_LUT_is_input_ordered, typename std::enable_if<!M>::type* = nullptr>
      void resize_inputs_2_merged_lut_if_needed(unsigned numInputGVCFs, unsigned numMergedFields)
      {	resize_and_reset_lut(m_inputs_2_merged_lut, numMergedFields, numInputGVCFs, m_num_merged_fields, m_num_input_vcfs); }

      template <bool M = merged_2_inputs_LUT_is_input_ordered, typename std::enable_if<M>::type* = nullptr>
      void resize_merged_2_inputs_lut_if_needed(unsigned numInputGVCFs, unsigned numMergedFields)
      {	resize_and_reset_lut(m_merged_2_inputs_lut, numInputGVCFs, numMergedFields, m_num_input_vcfs, m_num_merged_fields); }

      template <bool M = merged_2_inputs_LUT_is_input_ordered, typename std::enable_if<!M>::type* = nullptr>
      void resize_merged_2_inputs_lut_if_needed(unsigned numInputGVCFs, unsigned numMergedFields)
      {	resize_and_reset_lut(m_merged_2_inputs_lut, numMergedFields, numInputGVCFs, m_num_merged_fields, m_num_input_vcfs); }

      /*
       * @brief wrapper around single LUT resize functions
       */
      void resize_luts_if_needed(unsigned numInputGVCFs, unsigned numMergedFields)
      {
	resize_merged_2_inputs_lut_if_needed(numInputGVCFs, numMergedFields);
	resize_inputs_2_merged_lut_if_needed(numInputGVCFs, numMergedFields);
      }
      private:
      //why not unordered_map? because I feel the need, the need for speed
      std::vector<std::vector<int>> m_inputs_2_merged_lut;
      std::vector<std::vector<int>> m_merged_2_inputs_lut;
      /**
       * @brief invalidate/reset all mappings in a vector
       * @note sets all elements to missing
       * @param vec the vector to reset
       * @param from offset in the vector from which to start reset, 0 by default
       */
      void reset_vector(std::vector<int>& vec, unsigned from=0u);
      /**
       * @brief resize and reset a vector
       * @note resize and reset is done only if new_size > vec.size()
       */
      void resize_and_reset_vector(std::vector<int>& vec, unsigned new_size);
      /**
       * @brief resize and reset a LUT
       * @note resize and reset is done only if new_size > old_size
       */
      void resize_and_reset_lut(std::vector<std::vector<int>>& lut, unsigned new_lut_size, unsigned new_size, unsigned& numRowsVar, unsigned& numColsVar);

      /**
       * @brief get LUT value at a particular row,column
       * @note should be called only from the public wrapper functions get_*() as the wrappers take care of memory layout
       * @param lut LUT to access
       * @param rowIdx row
       * @param columnIdx column
       * @return value at lut[row][column], could be invalid, check with is_missing()
       */
      inline int get_lut_value(const std::vector<std::vector<int>>& lut, int rowIdx, int columnIdx) const
      {
	assert(rowIdx >= 0);
	assert(rowIdx < static_cast<int>(lut.size()));
	assert(columnIdx >= 0);
	assert(columnIdx < static_cast<int>(lut[rowIdx].size()));
	return lut[rowIdx][columnIdx];
      }

      /**
       * @brief set LUT value at a particular row,column
       * @note should be called only from the public wrapper functions add_input_merged_idx_pair() or reset_*() as the wrappers take care of memory layout
       * @param lut LUT to access
       * @param rowIdx row
       * @param columnIdx column
       * @param value value to write at lut[row][column] 
       */
      inline void set_lut_value(std::vector<std::vector<int>>& lut, int rowIdx, int columnIdx, int value)
      {
	assert(rowIdx >= 0);
	assert(rowIdx < static_cast<int>(lut.size()));
	assert(columnIdx >= 0);
	assert(columnIdx < static_cast<int>(lut[rowIdx].size()));
	lut[rowIdx][columnIdx] = value;
      }

      /**
       * @brief set merged field idx value (mergedIdx) corresponding to field idx inputIdx for input VCF inputGVCFIdx
       * @note should be called only from the public wrapper functions add_input_merged_idx_pair() or reset_*() as the wrappers take care of memory layout
       * @param inputGVCFIdx index of the input VCF file
       * @param inputIdx index of the field in the input VCF file
       * @param mergedIdx index of the field in the merged VCF file
       */
      template <bool M = inputs_2_merged_LUT_is_input_ordered, typename std::enable_if<M>::type* = nullptr>
      inline void set_merged_idx_for_input(unsigned inputGVCFIdx, int inputIdx, int mergedIdx)
      { set_lut_value(m_inputs_2_merged_lut, inputGVCFIdx, inputIdx, mergedIdx); } 

      template <bool M = inputs_2_merged_LUT_is_input_ordered, typename std::enable_if<!M>::type* = nullptr>
      inline void set_merged_idx_for_input(unsigned inputGVCFIdx, int inputIdx, int mergedIdx)
      { set_lut_value(m_inputs_2_merged_lut, inputIdx, inputGVCFIdx, mergedIdx); } 

      /**
       * @brief set input field idx value (inputIdx) for input VCF inputGVCFIdx corresponding to field idx mergedIdx in the merged VCF
       * @note should be called only from the public wrapper functions add_input_merged_idx_pair() or reset_*() as the wrappers take care of memory layout
       * @param inputGVCFIdx index of the input VCF file
       * @param inputIdx index of the field in the input VCF file
       * @param mergedIdx index of the field in the merged VCF file
       */
      template <bool M = merged_2_inputs_LUT_is_input_ordered, typename std::enable_if<M>::type* = nullptr>
      inline void set_input_idx_for_merged(unsigned inputGVCFIdx, int inputIdx, int mergedIdx)
      { set_lut_value(m_merged_2_inputs_lut, inputGVCFIdx, mergedIdx, inputIdx); }

      template <bool M = merged_2_inputs_LUT_is_input_ordered, typename std::enable_if<!M>::type* = nullptr>
      inline void set_input_idx_for_merged(unsigned inputGVCFIdx, int inputIdx, int mergedIdx)
      { set_lut_value(m_merged_2_inputs_lut, mergedIdx, inputGVCFIdx, inputIdx); }

    };

    /**
     * @brief LUT class for storing mappings between allele vectors in the merged file and input VCF files
     * Since the #alleles per site is expected to be small, this class sets the number of fields to 10. This makes any subsequent re-allocations
     * unlikely. The function resize_luts_if_needed() will almost always return immediately after failing the if condition
     */
    template<bool inputs_2_merged_LUT_is_input_ordered, bool merged_2_inputs_LUT_is_input_ordered>
    class MergedVCFAllelesIdxLUT
    : public MergedVCFLUTBase<inputs_2_merged_LUT_is_input_ordered, merged_2_inputs_LUT_is_input_ordered>
    {
      private:
	static const auto m_DEFAULT_INIT_NUM_ALLELES=10u;
      public:
	MergedVCFAllelesIdxLUT(unsigned numInputGVCFs)
	  : MergedVCFLUTBase<inputs_2_merged_LUT_is_input_ordered, merged_2_inputs_LUT_is_input_ordered>(numInputGVCFs,
	      m_DEFAULT_INIT_NUM_ALLELES)
	  { m_max_num_alleles = m_DEFAULT_INIT_NUM_ALLELES; }
	inline void resize_luts_if_needed(unsigned numMergedAlleles)
	{
	  if(numMergedAlleles > m_max_num_alleles)
	  {
	    MergedVCFLUTBase<inputs_2_merged_LUT_is_input_ordered, merged_2_inputs_LUT_is_input_ordered>::resize_luts_if_needed(
		MergedVCFLUTBase<inputs_2_merged_LUT_is_input_ordered, merged_2_inputs_LUT_is_input_ordered>::m_num_input_vcfs, numMergedAlleles); 
	    m_max_num_alleles = numMergedAlleles;
	  }
	}
      private:
	unsigned m_max_num_alleles;
    };

    /*NOTE: Needs explicit instantiation in .cpp file to use this type alias*/
    using CombineAllelesLUT = MergedVCFAllelesIdxLUT<true,true>;
  }
}

#endif
