#include "fftw3.h"
#include "include/ismrmrd.h"
#include <vector>
#include "include/xml.h"

class imageReconBase
{
public:

       
  void addAcquisition (ISMRMRD::Entity* ent);
  void addIsmrmrdHeader (ISMRMRD::IsmrmrdHeader hdr);
  
protected:

  ISMRMRD::IsmrmrdHeader    _header;
  bool                      _header_received;
  uint32_t                  _storage;
  bool                      _storage_set;

  std::vector<ISMRMRD::Acquisition<uint16_t>>  _acq16;
  std::vector<ISMRMRD::Acquisition<uint32_t>>  _acq32;
  std::vector<ISMRMRD::Acquisition<float>>     _acqFlt;
  std::vector<ISMRMRD::Acquisition<double>>    _acqDbl;
  
};
