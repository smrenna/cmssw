// user include files
#include <algorithm>
#include <vector>

#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/ConsumesCollector.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"

#include "FWCore/Utilities/interface/transform.h"

#include "DataFormats/HGCalReco/interface/TICLCandidate.h"
#include "DataFormats/HGCalReco/interface/Trackster.h"
#include "RecoHGCal/TICL/interface/TracksterMomentumPluginBase.h"
#include "RecoHGCal/TICL/interface/TracksterTrackPluginBase.h"

using namespace ticl;

class TICLCandidateFromTrackstersProducer : public edm::stream::EDProducer<> {
public:
  TICLCandidateFromTrackstersProducer(const edm::ParameterSet&);
  ~TICLCandidateFromTrackstersProducer() override {}
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

  void produce(edm::Event&, const edm::EventSetup&) override;

private:
  std::vector<edm::EDGetTokenT<std::vector<Trackster>>> trackster_tokens_;
  std::unique_ptr<TracksterMomentumPluginBase> momentum_algo_;
  std::unique_ptr<TracksterTrackPluginBase> track_algo_;
};
DEFINE_FWK_MODULE(TICLCandidateFromTrackstersProducer);

namespace {
  int pdg_id_from_idx(size_t i) {
    switch (i) {
      case 0:
        return 22;
      case 1:
        // Pick IDs with positive charge so they can be reset with charge * id downstream
        return -11;
      case 2:
        return -13;
      case 3:
        return 211;
      case 4:
        return 130;
    }
    return 0;
  }
}  // namespace

TICLCandidateFromTrackstersProducer::TICLCandidateFromTrackstersProducer(const edm::ParameterSet& ps) {
  trackster_tokens_ =
      edm::vector_transform(ps.getParameter<std::vector<edm::InputTag>>("tracksterCollections"),
                            [this](edm::InputTag const& tag) { return consumes<std::vector<Trackster>>(tag); });
  produces<std::vector<TICLCandidate>>();
  auto pset_momentum = ps.getParameter<edm::ParameterSet>("momentumPlugin");
  momentum_algo_ = TracksterMomentumPluginFactory::get()->create(
      pset_momentum.getParameter<std::string>("plugin"), pset_momentum, consumesCollector());

  auto pset_track = ps.getParameter<edm::ParameterSet>("trackPlugin");
  track_algo_ = TracksterTrackPluginFactory::get()->create(
      pset_track.getParameter<std::string>("plugin"), pset_track, consumesCollector());
}

void TICLCandidateFromTrackstersProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;

  std::vector<edm::InputTag> source_vector{
      edm::InputTag("trackstersTrk"), edm::InputTag("trackstersMIP"), edm::InputTag("tracksters")};
  desc.add<std::vector<edm::InputTag>>("tracksterCollections", source_vector);

  edm::ParameterSetDescription desc_momentum;
  desc_momentum.add<std::string>("plugin", "TracksterP4FromEnergySum");
  desc_momentum.add<bool>("energyFromRegression", false);
  desc_momentum.add<edm::InputTag>("vertices", edm::InputTag("offlinePrimaryVertices"));
  desc_momentum.add<edm::InputTag>("layerClusters", edm::InputTag("hgcalLayerClusters"));
  desc.add<edm::ParameterSetDescription>("momentumPlugin", desc_momentum);

  edm::ParameterSetDescription desc_track;
  desc_track.add<std::string>("plugin", "TracksterRecoTrackPlugin");
  desc.add<edm::ParameterSetDescription>("trackPlugin", desc_track);

  descriptions.add("ticlCandidateFromTrackstersProducer", desc);
}

void TICLCandidateFromTrackstersProducer::produce(edm::Event& evt, const edm::EventSetup& es) {
  auto result = std::make_unique<std::vector<TICLCandidate>>();

  std::vector<const Trackster*> trackster_ptrs;

  for (auto& trackster_token : trackster_tokens_) {
    edm::Handle<std::vector<Trackster>> trackster_h;
    evt.getByToken(trackster_token, trackster_h);
    size_t trackster_i = 0;
    for (auto const& trackster : *trackster_h) {
      trackster_ptrs.push_back(&trackster);
      result->emplace_back(edm::Ptr<ticl::Trackster>(trackster_h, trackster_i));
      ++trackster_i;
    }
  }

  // adds one TICLCandidate for each trackster
  momentum_algo_->setP4(trackster_ptrs, *result, evt);
  track_algo_->setTrack(trackster_ptrs, *result, evt);

  for (size_t i = 0; i < result->size(); ++i) {
    const auto& trackster = *trackster_ptrs[i];
    auto& ticl_cand = result->at(i);
    auto id_prob_begin = trackster.id_probabilities.begin();

    auto max_index = std::distance(id_prob_begin, std::max_element(id_prob_begin, trackster.id_probabilities.end()));
    auto pdg_id = pdg_id_from_idx(max_index);
    ticl_cand.setPdgId(pdg_id);

    if (ticl_cand.trackPtr().isNonnull()) {
      auto charge = ticl_cand.trackPtr()->charge();
      ticl_cand.setCharge(charge);
      ticl_cand.setPdgId(pdg_id * charge);
    } else if (pdg_id == -11 || pdg_id == -13 || pdg_id == 211) {
      // FIXME - placeholder for downstream PF code to work, but proper symmetric charge assignment needed
      ticl_cand.setCharge(1);
    }
  }

  evt.put(std::move(result));
}
