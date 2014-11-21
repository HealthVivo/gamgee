#include "merged_vcf_lut.h"

using namespace std;
namespace gamgee
{
  namespace utils
  {
    template<bool inputs_2_merged_LUT_is_input_ordered, bool merged_2_inputs_LUT_is_input_ordered>
    MergedVCFLUTBase<inputs_2_merged_LUT_is_input_ordered, merged_2_inputs_LUT_is_input_ordered>::MergedVCFLUTBase()
    {
      m_num_input_vcfs = 0u;
      m_num_merged_fields = 0u;
      clear();
    }

    template<bool inputs_2_merged_LUT_is_input_ordered, bool merged_2_inputs_LUT_is_input_ordered>
    MergedVCFLUTBase<inputs_2_merged_LUT_is_input_ordered, merged_2_inputs_LUT_is_input_ordered>::MergedVCFLUTBase(unsigned numInputGVCFs, unsigned numMergedFields)
    {
      m_num_input_vcfs = numInputGVCFs;
      m_num_merged_fields = numMergedFields;
      clear();
      resize_inputs_2_merged_lut_if_needed(numInputGVCFs, numMergedFields);
      resize_merged_2_inputs_lut_if_needed(numInputGVCFs, numMergedFields);
    }

    template<bool inputs_2_merged_LUT_is_input_ordered, bool merged_2_inputs_LUT_is_input_ordered>
    void MergedVCFLUTBase<inputs_2_merged_LUT_is_input_ordered, merged_2_inputs_LUT_is_input_ordered>::clear()
    {
      for(auto& vec : m_inputs_2_merged_lut)
        vec.clear();
      m_inputs_2_merged_lut.clear();
      for(auto& vec : m_merged_2_inputs_lut)
        vec.clear();
      m_merged_2_inputs_lut.clear();
    }

    template<bool inputs_2_merged_LUT_is_input_ordered, bool merged_2_inputs_LUT_is_input_ordered>
    void MergedVCFLUTBase<inputs_2_merged_LUT_is_input_ordered, merged_2_inputs_LUT_is_input_ordered>::reset_vector(vector<int>& vec, unsigned from)
    {
      for(auto i=from;i<vec.size();++i)
        vec[i] = gamgee::missing_values::int32;
    }

    template<bool inputs_2_merged_LUT_is_input_ordered, bool merged_2_inputs_LUT_is_input_ordered>
    void MergedVCFLUTBase<inputs_2_merged_LUT_is_input_ordered, merged_2_inputs_LUT_is_input_ordered>::resize_and_reset_vector(vector<int>& vec, unsigned new_size)
    {
      auto old_size = vec.size();
      if(new_size > old_size)
      {
        vec.resize(new_size);
        reset_vector(vec, old_size);
      }
    }

    template<bool inputs_2_merged_LUT_is_input_ordered, bool merged_2_inputs_LUT_is_input_ordered>
    void MergedVCFLUTBase<inputs_2_merged_LUT_is_input_ordered, merged_2_inputs_LUT_is_input_ordered>::resize_and_reset_lut
    (vector<vector<int>>& lut, unsigned new_lut_size, unsigned new_vector_size, unsigned& numRowsVar, unsigned& numColsVar)
    {
      auto old_lut_size = lut.size();
      if(new_lut_size > old_lut_size)
      {
        lut.resize(new_lut_size);
        numRowsVar = new_lut_size;
      }
      auto old_vector_size = (lut.size() > 0u) ? lut[0].size() : 0u;
      //Begin resizing of vectors at start_idx
      auto start_idx = old_lut_size;
      if(new_vector_size > old_vector_size)     //every vector needs to be resized
      {
        start_idx = 0u;
        numColsVar = new_vector_size;
      }
      else
        new_vector_size = old_vector_size;      //new vector size is smaller, don't bother reducing the size of existing rows
      for(auto i=start_idx;i<new_lut_size;++i)
        resize_and_reset_vector(lut[i], new_vector_size);
    }
    //explicit initialization to avoid link errors
    template class MergedVCFLUTBase<true,true>;
    template class MergedVCFLUTBase<true,false>;
    template class MergedVCFLUTBase<false,true>;
    template class MergedVCFLUTBase<false,false>;

    //explicit initialization to avoid link errors
    template class MergedVCFAllelesIdxLUT<true,true>;
  }
}
