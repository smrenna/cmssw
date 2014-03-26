#include <string>


#include "DataFormats/Candidate/interface/Candidate.h"
#include "DataFormats/ParticleFlowCandidate/interface/PFCandidate.h"
#include "DataFormats/ParticleFlowCandidate/interface/PFCandidateFwd.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/VertexReco/interface/VertexFwd.h"
#include "DataFormats/PatCandidates/interface/PackedCandidate.h"
#include "DataFormats/PatCandidates/interface/Jet.h"
#include "DataFormats/Common/interface/Association.h"
#include "FWCore/Framework/interface/EDProducer.h"
#include "DataFormats/Common/interface/View.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "DataFormats/GsfTrackReco/interface/GsfTrack.h"
#include "DataFormats/MuonReco/interface/Muon.h"

//#define CRAZYSORT 
//FIXME: debugging stuff to be removed
#define DEBUGIP 0
#if DEBUGIP
#include "TrackingTools/IPTools/interface/IPTools.h" 
#include "TrackingTools/TransientTrack/interface/TransientTrackBuilder.h"
#include "TrackingTools/Records/interface/TransientTrackRecord.h"

#include "TrackingTools/TrajectoryState/interface/TrajectoryStateOnSurface.h"
#include "TrackingTools/GeomPropagators/interface/AnalyticalTrajectoryExtrapolatorToLine.h"
#include "TrackingTools/GeomPropagators/interface/AnalyticalImpactPointExtrapolator.h"
#include "DataFormats/GeometryCommonDetAlgo/interface/Measurement1D.h"
#include "TrackingTools/TransientTrack/interface/TransientTrack.h"
#include "TrackingTools/IPTools/interface/IPTools.h"
#include "CLHEP/Vector/ThreeVector.h"
#include "CLHEP/Vector/LorentzVector.h"
#include "CLHEP/Matrix/Vector.h"
#include <string>

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "RecoVertex/VertexTools/interface/VertexDistance3D.h"
#include "RecoVertex/VertexTools/interface/VertexDistanceXY.h"
#include "RecoVertex/VertexPrimitives/interface/ConvertToFromReco.h"
#endif

namespace pat {
    class PATPackedCandidateProducer : public edm::EDProducer {
        public:
            explicit PATPackedCandidateProducer(const edm::ParameterSet&);
            ~PATPackedCandidateProducer();

            virtual void produce(edm::Event&, const edm::EventSetup&);

        private:
            edm::EDGetTokenT<reco::PFCandidateCollection>    Cands_;
            edm::EDGetTokenT<reco::PFCandidateFwdPtrVector>  CandsFromPVLoose_;
            edm::EDGetTokenT<reco::PFCandidateFwdPtrVector>  CandsFromPVTight_;
            edm::EDGetTokenT<reco::VertexCollection>         PVs_;
            edm::EDGetTokenT<reco::VertexCollection>         PVOrigs_;
            double minPtForTrackProperties_;
            // for debugging
            float calcDxy(float dx, float dy, float phi) {
                return - dx * std::sin(phi) + dy * std::cos(phi);
            }
            float calcDz(reco::Candidate::Point p, reco::Candidate::Point v, const reco::Candidate &c) {
                return p.Z()-v.Z() - ((p.X()-v.X()) * c.px() + (p.Y()-v.Y())*c.py()) * c.pz()/(c.pt()*c.pt());
            }
    };
}

pat::PATPackedCandidateProducer::PATPackedCandidateProducer(const edm::ParameterSet& iConfig) :
  Cands_(consumes<reco::PFCandidateCollection>(iConfig.getParameter<edm::InputTag>("inputCollection"))),
  CandsFromPVLoose_(consumes<reco::PFCandidateFwdPtrVector>(iConfig.getParameter<edm::InputTag>("inputCollectionFromPVLoose"))),
  CandsFromPVTight_(consumes<reco::PFCandidateFwdPtrVector>(iConfig.getParameter<edm::InputTag>("inputCollectionFromPVTight"))),
  PVs_(consumes<reco::VertexCollection>(iConfig.getParameter<edm::InputTag>("inputVertices"))),
  PVOrigs_(consumes<reco::VertexCollection>(iConfig.getParameter<edm::InputTag>("originalVertices"))),
  minPtForTrackProperties_(iConfig.getParameter<double>("minPtForTrackProperties"))
{
  produces< std::vector<pat::PackedCandidate> > ();
  produces< edm::Association<pat::PackedCandidateCollection> > ();
  produces< edm::Association<reco::PFCandidateCollection> > ();
}

pat::PATPackedCandidateProducer::~PATPackedCandidateProducer() {}

void pat::PATPackedCandidateProducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {

#ifdef CRAZYSORT 
    edm::Handle<edm::View<pat::Jet> >      jets;
    iEvent.getByLabel("selectedPatJets", jets);
#endif


    edm::Handle<reco::PFCandidateCollection> cands;
    iEvent.getByToken( Cands_, cands );
    std::vector<reco::Candidate>::const_iterator cand;

    edm::Handle<reco::PFCandidateFwdPtrVector> candsFromPVLoose;
    iEvent.getByToken( CandsFromPVLoose_, candsFromPVLoose );
    edm::Handle<reco::PFCandidateFwdPtrVector> candsFromPVTight;
    iEvent.getByToken( CandsFromPVTight_, candsFromPVTight );

    std::vector<pat::PackedCandidate::PVAssoc> fromPV(cands->size(), pat::PackedCandidate::NoPV);
    for (const reco::PFCandidateFwdPtr &ptr : *candsFromPVLoose) {
        if (ptr.ptr().id() == cands.id()) {
            fromPV[ptr.ptr().key()]   = pat::PackedCandidate::PVLoose;
        } else if (ptr.backPtr().id() == cands.id()) {
            fromPV[ptr.backPtr().key()] = pat::PackedCandidate::PVLoose;
        } else {
            throw cms::Exception("Configuration", "The elements from 'inputCollectionFromPVLoose' don't point to 'inputCollection'\n");
        }
    }
    for (const reco::PFCandidateFwdPtr &ptr : *candsFromPVTight) {
        if (ptr.ptr().id() == cands.id()) {
            fromPV[ptr.ptr().key()]   = pat::PackedCandidate::PVTight;
        } else if (ptr.backPtr().id() == cands.id()) {
            fromPV[ptr.backPtr().key()] = pat::PackedCandidate::PVTight;
        } else {
            throw cms::Exception("Configuration", "The elements from 'inputCollectionFromPVTight' don't point to 'inputCollection'\n");
        }
    }

    edm::Handle<reco::VertexCollection> PVOrigs;
    iEvent.getByToken( PVOrigs_, PVOrigs );
    const reco::Vertex & PVOrig = (*PVOrigs)[0];
    edm::Handle<reco::VertexCollection> PVs;
    iEvent.getByToken( PVs_, PVs );
    reco::VertexRef PV(PVs.id());
    math::XYZPoint  PVpos;
    if (!PVs->empty()) {
        PV = reco::VertexRef(PVs, 0);
        PVpos = PV->position();
    }

    std::auto_ptr< std::vector<pat::PackedCandidate> > outPtrP( new std::vector<pat::PackedCandidate> );
    std::vector<int> mapping(cands->size());
#ifdef CRAZYSORT
    std::vector<int> jetOrder;
    std::vector<int> jetOrderReverse;
    for(unsigned int i=0;i<cands->size();i++) jetOrderReverse.push_back(-1);
    for (edm::View<pat::Jet>::const_iterator it = jets->begin(), ed = jets->end(); it != ed; ++it) {
      const  pat::Jet & jet = *it;
      const  reco::CompositePtrCandidate::daughters & dau=jet.daughterPtrVector();
      for(unsigned int  i=0;i<dau.size();i++)
	{
           if((*cands)[dau[i].key()].trackRef().isNonnull() && (*cands)[dau[i].key()].pt() > minPtForTrackProperties_){
	   jetOrder.push_back(dau[i].key());
	   jetOrderReverse[jetOrder.back()]=jetOrder.size()-1;
	   }
	}
      for(unsigned int  i=0;i<dau.size();i++)
        {
           if(!((*cands)[dau[i].key()].trackRef().isNonnull() && (*cands)[dau[i].key()].pt() > minPtForTrackProperties_)){
           jetOrder.push_back(dau[i].key());
           jetOrderReverse[jetOrder.back()]=jetOrder.size()-1;
           }
        }

    }
   for(unsigned int ic=0, nc = cands->size(); ic < nc; ++ic) {
	if(jetOrderReverse[ic]==-1 && (*cands)[ic].trackRef().isNonnull() && (*cands)[ic].pt() > minPtForTrackProperties_)
        {
           jetOrder.push_back(ic);
           jetOrderReverse[jetOrder.back()]=jetOrder.size()-1;
        }

   }
  //all what's left
   for(unsigned int ic=0, nc = cands->size(); ic < nc; ++ic) {
        if(jetOrderReverse[ic]==-1)
        {
           jetOrder.push_back(ic);
           jetOrderReverse[jetOrder.back()]=jetOrder.size()-1;
        }

   }
#endif //CRAZYSORT


    for(unsigned int ic=0, nc = cands->size(); ic < nc; ++ic) {
#ifdef CRAZYSORT
        const reco::PFCandidate &cand=(*cands)[jetOrder[ic]];
#else
        const reco::PFCandidate &cand=(*cands)[ic];
#endif
        float phiAtVtx = cand.phi();
        /*bool flags = false;
        if (cand.flag(reco::PFCandidate::T_TO_DISP)   || 
            cand.flag(reco::PFCandidate::T_FROM_DISP) ||
            cand.flag(reco::PFCandidate::T_FROM_V0)   ||
            cand.flag(reco::PFCandidate::T_FROM_GAMMACONV)) {
            flags = true;
            std::cout << "Candidate has flags!" ;
            if( cand.flag( reco::PFCandidate::T_FROM_DISP ) ) std::cout << " T_FROM_DISP";
            if( cand.flag( reco::PFCandidate::T_TO_DISP ) ) std::cout << " T_TO_DISP";
            if( cand.flag( reco::PFCandidate::T_FROM_GAMMACONV ) ) std::cout << " T_FROM_GAMMACONV";
            if( cand.flag( reco::PFCandidate::GAMMA_TO_GAMMACONV ) ) std::cout << " GAMMA_TO_GAMMACONV";
            std::cout << std::endl;
        }*/
        if (cand.charge()) {
            math::XYZPoint vtx = cand.vertex();
            //float dxyBefore = 0, dyBefore = 0;
            if (abs(cand.pdgId()) == 11 && cand.gsfTrackRef().isNonnull()) {
                /*if (cand.vertexType() != reco::PFCandidate::kGSFVertex) {
                     std::cout << "Candidate electron vertex type is " << cand.vertexType() << std::endl; 
                     flags = true;
                }*/
                vtx = cand.gsfTrackRef()->referencePoint();
                phiAtVtx = cand.gsfTrackRef()->phi();
	        
                //dxyBefore = cand.gsfTrackRef()->dxy(PVpos);
                //dzBefore = cand.gsfTrackRef()->dz(PVpos);
            } else if (cand.trackRef().isNonnull()) {
                /*if (cand.vertexType() != reco::PFCandidate::kTrkVertex) {
                     std::cout << "Candidate track vertex type is " << cand.vertexType() << std::endl; 
                     flags = true;
                }*/
                vtx = cand.trackRef()->referencePoint();
                phiAtVtx = cand.trackRef()->phi();
                //dxyBefore = cand.trackRef()->dxy(PVpos);
                //dzBefore = cand.trackRef()->dz(PVpos);
            } else {
                //dxyBefore = calcDxy(vtx.X()-PVpos.X(),vtx.Y()-PVpos.Y(),cand.phi());
                //dzBefore = calcDz(vtx,PVpos,cand);
            }
            outPtrP->push_back( pat::PackedCandidate(cand.polarP4(), vtx, phiAtVtx, cand.pdgId(), PV, fromPV[ic]));
	    if(cand.trackRef().isNonnull()){
		if(PVOrig.trackWeight(cand.trackRef()) > 0.5)  outPtrP->back().setFromPV(pat::PackedCandidate::PVUsedInFit);
	    }	
	    if(cand.trackRef().isNonnull() && cand.pt() > minPtForTrackProperties_)
	    {
		outPtrP->back().setTrackProperties(*cand.trackRef());

///// DEBUG
#if DEBUGIP
		if( fabs(cand.trackRef()->dz()-PV->position().z()) < 0.3 && cand.trackRef()->numberOfValidHits() > 7 ){
		reco::Track tr = outPtrP->back().pseudoTrack();
		edm::ESHandle<TransientTrackBuilder> builder;
		iSetup.get<TransientTrackRecord>().get("TransientTrackBuilder", builder);		

		reco::TransientTrack newTT = builder->build(tr);
		reco::TransientTrack oldTT = builder->build(*cand.trackRef());
		Measurement1D ip3Dnew = (IPTools::absoluteImpactParameter3D(newTT,*PV)).second;
		Measurement1D ip3Dold = (IPTools::absoluteImpactParameter3D(oldTT,*PV)).second;
		if(tr.hitPattern().numberOfValidHits() != cand.trackRef()->hitPattern().numberOfValidHits() || tr.hitPattern().numberOfValidPixelHits() != cand.trackRef()->hitPattern().numberOfValidPixelHits() )
		{
			std::cout << tr.hitPattern().numberOfValidHits() << " "<<  cand.trackRef()->hitPattern().numberOfValidHits() << " " << tr.hitPattern().numberOfValidPixelHits() << " " << cand.trackRef()->hitPattern().numberOfValidPixelHits()<< std::endl;
		}
		if( 		  ( fabs(ip3Dnew.significance()-ip3Dold.significance())/ip3Dold.significance() > 0.02 && ip3Dold.significance() < 10 )
		||  ( fabs(ip3Dnew.significance()-ip3Dold.significance())/ip3Dold.significance() > 0.10 && ip3Dold.significance() > 10 )
		) {
		std::cout <<" NEW vs OLD  : " <<  ip3Dnew.value() << " / " << ip3Dnew.error() << " = " << ip3Dnew.significance() << " vs " << ip3Dold.value() << " / " << ip3Dold.error() << " = " << ip3Dold.significance() <<  " pt: " << cand.pt() <<  " eta: " << cand.eta() << " pdg: " << cand.pdgId() <<   std::endl;

		std::cout << "new covariance" <<  std::endl << tr.covariance() << std::endl;
		std::cout <<  "old covariance" <<  std::endl  << cand.trackRef()->covariance() << std::endl;
		const reco::Vertex & vertex = *PV;

		{
		using namespace reco;
		std::cout<< "new math" << std::endl;
		const reco::TransientTrack & transientTrack = newTT;
		AnalyticalImpactPointExtrapolator extrapolator(transientTrack.field());
		TrajectoryStateOnSurface tsos = extrapolator.extrapolate(transientTrack.impactPointState(), RecoVertex::convertPos(vertex.position()));
	
	        GlobalPoint refPoint          = tsos.globalPosition();
  //      	GlobalError refPointErr       = tsos.cartesianError().position();
	        GlobalPoint vertexPosition    = RecoVertex::convertPos(vertex.position());
		std::cout << GlobalVector(refPoint-vertexPosition).eta() << std::endl;
  //      	GlobalError vertexPositionErr = RecoVertex::convertError(vertex.error());
		std::cout << tsos  << std::endl;
		}

                {
		using namespace reco;
                std::cout<< "old math" << std::endl;
                const reco::TransientTrack & transientTrack = oldTT;
                AnalyticalImpactPointExtrapolator extrapolator(transientTrack.field());
		TrajectoryStateOnSurface tsos = extrapolator.extrapolate(transientTrack.impactPointState(), RecoVertex::convertPos(vertex.position()));
		std::cout << tsos  << std::endl;
//                GlobalPoint refPoint          = tsos.globalPosition();
  ///              GlobalError refPointErr       = tsos.cartesianError().position();
     //           GlobalPoint vertexPosition    = RecoVertex::convertPos(vertex.position());
       //         GlobalError vertexPositionErr = RecoVertex::convertError(vertex.error());
//                std::cout << refPoint << std::endl << refPointErr << std::endl;
                }


		} else {std::cout << " OK " << std::endl;}
		}
#endif
//// ENDDEBUG
	    }	
            /*if (flags) {
            const pat::PackedCandidate &pc = outPtrP->back();
            //float dxyAfter = pc.dz(); // .dxy(); //calcDxy(pc.vx()-PVpos.X(),pc.vy()-PVpos.Y(),pc.phi());
            //if (std::abs(dxyBefore-dxyAfter)/(std::abs(dxyBefore)+std::abs(dxyAfter)+0.0001) > 1e-3) {
                if (abs(cand.pdgId()) == 11 && cand.gsfTrackRef().isNonnull()) {
                    printf("XYZ of cand before:   %+18.10f   %+18.10f   %+18.10f      PHI = %+12.10f   DXY = %+18.10f  DZ = %+18.10f  [ pdgId %+4d ]\n", cand.vertex().X(), cand.vertex().Y(), cand.vertex().Z(), cand.phi(), calcDxy(cand.vertex().X()-PVpos.X(),cand.vertex().Y()-PVpos.Y(),cand.phi()), calcDz(vtx,PVpos,cand), cand.pdgId());
                    printf("XYZ of gsft before:   %+18.10f   %+18.10f   %+18.10f      PHI = %+12.10f   DXY = %+18.10f  DZ = %+18.10f  [ pdgId %+4d ]\n", vtx.X(), vtx.Y(), vtx.Z(), cand.gsfTrackRef()->phi(), cand.gsfTrackRef()->dxy(PVpos), cand.gsfTrackRef()->dz(PVpos), cand.pdgId());
                } else if (cand.trackRef().isNonnull()) {
                    printf("XYZ of cand before:   %+18.10f   %+18.10f   %+18.10f      PHI = %+12.10f   DXY = %+18.10f  DZ = %+18.10f  [ pdgId %+4d ]\n", cand.vertex().X(), cand.vertex().Y(), cand.vertex().Z(), cand.phi(), calcDxy(cand.vertex().X()-PVpos.X(),cand.vertex().Y()-PVpos.Y(),cand.phi()), calcDz(vtx,PVpos,cand), cand.pdgId());
                    printf("XYZ of trk  before:   %+18.10f   %+18.10f   %+18.10f      PHI = %+12.10f   DXY = %+18.10f  DZ = %+18.10f  [ pdgId %+4d ]\n", vtx.X(), vtx.Y(), vtx.Z(), cand.trackRef()->phi(), cand.trackRef()->dxy(PVpos), cand.trackRef()->dz(PVpos), cand.pdgId());
                } else {
                    printf("XYZ of cand before:   %+18.10f   %+18.10f   %+18.10f      PHI = %+12.10f   DXY = %+18.10f  DZ = %+18.10f  [ pdgId %+4d ]\n", vtx.X(), vtx.Y(), vtx.Z(), cand.phi(), calcDxy(vtx.X()-PVpos.X(),vtx.Y()-PVpos.Y(),cand.phi()), calcDz(vtx,PVpos,cand), cand.pdgId());
                }
                    printf("XYZ of cand after:    %+18.10f   %+18.10f   %+18.10f      PHI = %+12.10f   DXY = %+18.10f  DZ = %+18.10f  \n",  pc.vx(), pc.vy(), pc.vz(), pc.phi(), calcDxy(pc.vx()-PVpos.X(),pc.vy()-PVpos.Y(),pc.phi()), calcDz(vtx,PVpos,pc));
                    printf("corrected dxy, dz:                                                                      PHI = %+12.10f   DXY = %+18.10f  DZ = %+18.10f\n",                             pc.phiAtVtx(), pc.dxy(), pc.dz());
            }*/
        } else {
            outPtrP->push_back( pat::PackedCandidate(cand.polarP4(), PVpos, cand.phi(), cand.pdgId(), PV, fromPV[ic]));
            /*if (cand.vertexType() != reco::PFCandidate::kCandVertex) {
                math::XYZPoint vtx = cand.vertex();
                const pat::PackedCandidate &pc = outPtrP->back();
                std::cout << "Candidate neutral vertex type is " << cand.vertexType() << std::endl; 
                printf("XYZ of cand before:   %+18.10f   %+18.10f   %+18.10f      PHI = %+12.10f   DXY = %+18.10f  DZ = %+18.10f  [ pdgId %+4d ]\n", cand.vertex().X(), cand.vertex().Y(), cand.vertex().Z(), cand.phi(), calcDxy(cand.vertex().X()-PVpos.X(),cand.vertex().Y()-PVpos.Y(),cand.phi()), calcDz(vtx,PVpos,cand), cand.pdgId());
                printf("XYZ of cand after:    %+18.10f   %+18.10f   %+18.10f      PHI = %+12.10f   DXY = %+18.10f  DZ = %+18.10f  \n",  pc.vx(), pc.vy(), pc.vz(), pc.phi(), calcDxy(pc.vx()-PVpos.X(),pc.vy()-PVpos.Y(),pc.phi()), calcDz(vtx,PVpos,pc));
                printf("corrected dxy, dz:                                                                      PHI = %+12.10f   DXY = %+18.10f  DZ = %+18.10f\n",                             pc.phiAtVtx(), pc.dxy(), pc.dz());
            }*/
        }

        mapping[ic] = ic; // trivial at the moment!
    }


    edm::OrphanHandle<pat::PackedCandidateCollection> oh = iEvent.put( outPtrP );

    // now build the two maps
    std::auto_ptr<edm::Association<pat::PackedCandidateCollection> > pf2pc(new edm::Association<pat::PackedCandidateCollection>(oh   ));
    std::auto_ptr<edm::Association<reco::PFCandidateCollection   > > pc2pf(new edm::Association<reco::PFCandidateCollection   >(cands));
    edm::Association<pat::PackedCandidateCollection>::Filler pf2pcFiller(*pf2pc);
    edm::Association<reco::PFCandidateCollection   >::Filler pc2pfFiller(*pc2pf);
#ifdef CRAZYSORT
    pf2pcFiller.insert(cands, jetOrderReverse.begin(), jetOrderReverse.end());
    pc2pfFiller.insert(oh   , jetOrder.begin(), jetOrder.end());
#else
    pf2pcFiller.insert(cands, mapping.begin(), mapping.end());
    pc2pfFiller.insert(oh   , mapping.begin(), mapping.end());
#endif
    pf2pcFiller.fill();
    pc2pfFiller.fill();
    iEvent.put(pf2pc);
    iEvent.put(pc2pf);

}


using pat::PATPackedCandidateProducer;
#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(PATPackedCandidateProducer);
