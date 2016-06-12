#ifndef IMAGERECONBASE_H
#define IMAGERECONBASE_H

#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"

class imageReconBase
{
  public:
  virtual                   ~imageReconBase() = default;
  virtual ISMRMRD::Entity*  runReconstruction (std::vector<ISMRMRD::Entity*>& acqs,
                                               ISMRMRD::StorageType stype,
                                               ISMRMRD::IsmrmrdHeader& hdr) = 0;
  //virtual void addAcquisition (ISMRMRD::Entity* ent, uint32_t storage) = 0;
  //virtual void addIsmrmrdHeader (ISMRMRD::IsmrmrdHeader hdr) = 0;
  //virtual bool isImageDone () = 0;

  //virtual ISMRMRD::Entity* getImageEntityPointer () = 0;
 };

#endif //IMAGERECONBASE_H
