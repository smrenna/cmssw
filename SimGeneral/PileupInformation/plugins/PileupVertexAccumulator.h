#ifndef PileupVertexAccumulator_h
#define PileupVertexAccumulator_h

/** \class PileupVertexAccumulator
 *
 * PileupVertexAccumulator saves some pileup vertex information which is passed to
 * PileupSummaryInformation
 *
 * \author Mike Hildreth
 *
 * \version   Jan 22 2015  

 *
 ************************************************************/

#include <memory>
#include <string>
#include <vector>

#include "SimGeneral/MixingModule/interface/DigiAccumulatorMixMod.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "DataFormats/Provenance/interface/EventID.h"
#include "FWCore/Utilities/interface/InputTag.h"

namespace edm {
  class ConsumesCollector;
  namespace one {
    class EDProducerBase;
  }
  class Event;
  class EventSetup;
  class ParameterSet;
  template<typename T> class Handle;
  class StreamID;
}

class PileUpEventPrincipal;

namespace cms {
  class PileupVertexAccumulator : public DigiAccumulatorMixMod {
  public:

    explicit PileupVertexAccumulator(const edm::ParameterSet& conf, edm::stream::EDProducerBase& mixMod, edm::ConsumesCollector& iC);

    ~PileupVertexAccumulator() override;

    void initializeEvent(edm::Event const& e, edm::EventSetup const& c) override;
    void accumulate(edm::Event const& e, edm::EventSetup const& c) override;
    void accumulate(PileUpEventPrincipal const& e, edm::EventSetup const& c, edm::StreamID const&) override;
    void finalizeEvent(edm::Event& e, edm::EventSetup const& c) override;

    virtual void beginJob() {}

  private:
    std::vector<float> pT_Hats_;
    std::vector<float> z_posns_;
    std::vector<float> t_posns_;
    edm::InputTag Mtag_;
    edm::InputTag fallbackMtag_;
    bool saveVtxTimes_;

  };
}


#endif
