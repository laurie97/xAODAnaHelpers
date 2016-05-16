// c++ include(s):
#include <iostream>

// EL include(s):
#include <EventLoop/Job.h>
#include <EventLoop/StatusCode.h>
#include <EventLoop/Worker.h>

// EDM include(s):
#include "xAODEventInfo/EventInfo.h"
#include "xAODMuon/MuonContainer.h"
#include "xAODMuon/Muon.h"
#include "xAODBase/IParticleHelpers.h"
#include "xAODBase/IParticleContainer.h"
#include "xAODBase/IParticle.h"
#include "AthContainers/ConstDataVector.h"
#include "AthContainers/DataVector.h"

// package include(s):
#include "xAODAnaHelpers/HelperFunctions.h"
#include "xAODAnaHelpers/HelperClasses.h"
#include "xAODAnaHelpers/MuonEfficiencyCorrector.h"

#include <xAODAnaHelpers/tools/ReturnCheck.h>

using HelperClasses::ToolName;

// this is needed to distribute the algorithm to the workers
ClassImp(MuonEfficiencyCorrector)


MuonEfficiencyCorrector :: MuonEfficiencyCorrector (std::string className) :
    Algorithm(className),
    m_muRecoSF_tool_handle("CP::MuonEfficiencyScaleFactors/MuRecoSFToolName", nullptr),
    m_muIsoSF_tool_handle("CP::MuonEfficiencyScaleFactors/MuIsoSFToolName", nullptr),
    m_muTrigSF_tool_handle("CP::MuonTriggerScaleFactors/MuTrigSFToolName", nullptr),
    m_muTTVASF_tool_handle("CP::MuonEfficiencyScaleFactors/MuTTVASFToolName", nullptr),
    m_pileup_tool_handle("CP::PileupReweightingTool/PileupToolName", nullptr)
{
  // Here you put any code for the base initialization of variables,
  // e.g. initialize all pointers to 0.  Note that you should only put
  // the most basic initialization here, since this method will be
  // called on both the submission and the worker node.  Most of your
  // initialization code will go into histInitialize() and
  // initialize().

  Info("MuonEfficiencyCorrector()", "Calling constructor");

  m_debug                      = false;

  // Input container to be read from TEvent or TStore
  //
  m_inContainerName            = "";

  m_calibRelease               = "Data15_allPeriods_241115";

  // Reco efficiency SF
  //
  m_WorkingPointReco           = "Loose";

  // Iso efficiency SF
  //
  m_WorkingPointIso            = "LooseTrackOnly";

  // Trigger efficiency SF
  //
  m_runNumber                  = 276329;
  m_useRandomRunNumber         = true;

  m_WorkingPointRecoTrig       = "Loose";
  m_WorkingPointIsoTrig        = "LooseTrackOnly";
  m_SingleMuTrig               = "HLT_mu20_iloose_L1MU15";
  m_DiMuTrig                   = "HLT_2mu14";

  // TTVA SF
  //
  m_WorkingPointTTVA           = "TTVA";

  // Systematics stuff
  //
  m_inputAlgoSystNames         = "";
  m_systValReco 	       = 0.0;
  m_systValIso 	               = 0.0;
  m_systValTrig 	       = 0.0;
  m_systValTTVA 	       = 0.0;
  m_systNameReco	       = "";
  m_systNameIso	               = "";
  m_systNameTrig	       = "";
  m_systNameTTVA	       = "";
  m_outputSystNamesReco        = "MuonEfficiencyCorrector_RecoSyst";
  m_outputSystNamesIso         = "MuonEfficiencyCorrector_IsoSyst";
  m_outputSystNamesTrig        = "MuonEfficiencyCorrector_TrigSyst";
  m_outputSystNamesTrigMCEff   = "MuonEfficiencyCorrector_TrigMCEff";
  m_outputSystNamesTTVA        = "MuonEfficiencyCorrector_TTVASyst";

}


EL::StatusCode MuonEfficiencyCorrector :: setupJob (EL::Job& job)
{
  // Here you put code that sets up the job on the submission object
  // so that it is ready to work with your algorithm, e.g. you can
  // request the D3PDReader service or add output files.  Any code you
  // put here could instead also go into the submission script.  The
  // sole advantage of putting it here is that it gets automatically
  // activated/deactivated when you add/remove the algorithm from your
  // job, which may or may not be of value to you.

  Info("setupJob()", "Calling setupJob");

  job.useXAOD ();
  xAOD::Init( "MuonEfficiencyCorrector" ).ignore(); // call before opening first file

  return EL::StatusCode::SUCCESS;
}



EL::StatusCode MuonEfficiencyCorrector :: histInitialize ()
{
  // Here you do everything that needs to be done at the very
  // beginning on each worker node, e.g. create histograms and output
  // trees.  This method gets called before any input files are
  // connected.
  RETURN_CHECK("xAH::Algorithm::algInitialize()", xAH::Algorithm::algInitialize(), "");
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode MuonEfficiencyCorrector :: fileExecute ()
{
  // Here you do everything that needs to be done exactly once for every
  // single file, e.g. collect a list of all lumi-blocks processed
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode MuonEfficiencyCorrector :: changeInput (bool /*firstFile*/)
{
  // Here you do everything you need to do when we change input files,
  // e.g. resetting branch addresses on trees.  If you are using
  // D3PDReader or a similar service this method is not needed.
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode MuonEfficiencyCorrector :: initialize ()
{
  // Here you do everything that you need to do after the first input
  // file has been connected and before the first event is processed,
  // e.g. create additional histograms based on which variables are
  // available in the input files.  You can also create all of your
  // histograms and trees in here, but be aware that this method
  // doesn't get called if no events are processed.  So any objects
  // you create here won't be available in the output if you have no
  // input events.

  Info("initialize()", "Initializing MuonEfficiencyCorrector Interface... ");

  m_event = wk()->xaodEvent();
  m_store = wk()->xaodStore();

  Info("initialize()", "Number of events in file: %lld ", m_event->getEntries() );

  if ( m_inContainerName.empty() ) {
    Error("initialize()", "InputContainer is empty!");
    return EL::StatusCode::FAILURE;
  }


  const xAOD::EventInfo* eventInfo(nullptr);
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", HelperFunctions::retrieve(eventInfo, m_eventInfoContainerName, m_event, m_store, m_verbose) ,"");
  m_isMC = ( eventInfo->eventType( xAOD::EventInfo::IS_SIMULATION ) );

  m_numEvent      = 0;
  m_numObject     = 0;

  // 1.
  // initialize the CP::MuonEfficiencyScaleFactors Tool for reco efficiency SF
  //
  m_recoEffSF_tool_name = "MuonEfficiencyScaleFactors_effSF_Reco_" + m_WorkingPointReco;
  std::string recoEffSF_handle_name = "CP::MuonEfficiencyScaleFactors/" + m_recoEffSF_tool_name;

  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", checkToolStore<CP::MuonEfficiencyScaleFactors>(m_recoEffSF_tool_name), "" );
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muRecoSF_tool_handle.makeNew<CP::MuonEfficiencyScaleFactors>(recoEffSF_handle_name), "Failed to create handle to CP::MuonEfficiencyScaleFactors for reco efficiency SF");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muRecoSF_tool_handle.setProperty("WorkingPoint", m_WorkingPointReco ),"Failed to set Working Point property of MuonEfficiencyScaleFactors for reco efficiency SF");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muRecoSF_tool_handle.setProperty("CalibrationRelease", m_calibRelease ),"Failed to set calibration release property of MuonEfficiencyScaleFactors for reco efficiency SF");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muRecoSF_tool_handle.initialize(), "Failed to properly initialize CP::MuonEfficiencyScaleFactors for reco efficiency SF");

  //  Add the chosen WP to the string labelling the vector<SF> decoration
  //
  m_outputSystNamesReco = m_outputSystNamesReco + "_" + m_WorkingPointReco;

  if ( m_debug ) {
    CP::SystematicSet affectSystsReco = m_muRecoSF_tool_handle->affectingSystematics();
    for ( const auto& syst_it : affectSystsReco ) { Info("initialize()","MuonEfficiencyScaleFactors tool can be affected by reco efficiency systematic: %s", (syst_it.name()).c_str()); }
  }
  //
  // Make a list of systematics to be used, based on configuration input
  // Use HelperFunctions::getListofSystematics() for this!
  //
  const CP::SystematicSet recSystsReco = m_muRecoSF_tool_handle->recommendedSystematics();
  m_systListReco = HelperFunctions::getListofSystematics( recSystsReco, m_systNameReco, m_systValReco, m_debug );

  Info("initialize()","Will be using MuonEfficiencyScaleFactors tool reco efficiency systematic:");
  for ( const auto& syst_it : m_systListReco ) {
    if ( m_systNameReco.empty() ) {
      Info("initialize()","\t Running w/ nominal configuration only!");
      break;
    }
    Info("initialize()","\t %s", (syst_it.name()).c_str());
  }

  // 2.
  // initialize the CP::MuonEfficiencyScaleFactors Tool for isolation efficiency SF
  //

  m_isoEffSF_tool_name = "MuonEfficiencyScaleFactors_effSF_Iso_" + m_WorkingPointIso;
  std::string isoEffSF_handle_name = "CP::MuonEfficiencyScaleFactors/" + m_isoEffSF_tool_name;

  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", checkToolStore<CP::MuonEfficiencyScaleFactors>(m_isoEffSF_tool_name), "" );
  std::string tool_WP = m_WorkingPointIso + "Iso";
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muIsoSF_tool_handle.makeNew<CP::MuonEfficiencyScaleFactors>(isoEffSF_handle_name), "Failed to create handle to CP::MuonEfficiencyScaleFactors for iso efficiency SF");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muIsoSF_tool_handle.setProperty("WorkingPoint", tool_WP ),"Failed to set Working Point property of MuonEfficiencyScaleFactors for iso efficiency SF");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muIsoSF_tool_handle.setProperty("CalibrationRelease", m_calibRelease ),"Failed to set calibration release property of MuonEfficiencyScaleFactors for iso efficiency SF");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muIsoSF_tool_handle.initialize(), "Failed to properly initialize CP::MuonEfficiencyScaleFactors for iso efficiency SF");

  //  Add the chosen WP to the string labelling the vector<SF> decoration
  //
  m_outputSystNamesIso = m_outputSystNamesIso + "_Iso" + m_WorkingPointIso;

  if ( m_debug ) {
    CP::SystematicSet affectSystsIso = m_muIsoSF_tool_handle->affectingSystematics();
    for ( const auto& syst_it : affectSystsIso ) { Info("initialize()","MuonEfficiencyScaleFactors tool can be affected by iso efficiency systematic: %s", (syst_it.name()).c_str()); }
  }
  //
  // Make a list of systematics to be used, based on configuration input
  // Use HelperFunctions::getListofSystematics() for this!
  //
  const CP::SystematicSet recSystsIso = m_muIsoSF_tool_handle->recommendedSystematics();
  m_systListIso = HelperFunctions::getListofSystematics( recSystsIso, m_systNameIso, m_systValIso, m_debug );

  Info("initialize()","Will be using MuonEfficiencyScaleFactors tool iso efficiency systematic:");
  for ( const auto& syst_it : m_systListIso ) {
    if ( m_systNameIso.empty() ) {
      Info("initialize()","\t Running w/ nominal configuration only!");
      break;
    }
    Info("initialize()","\t %s", (syst_it.name()).c_str());
  }

  // 3.
  // Initialise the CP::MuonTriggerScaleFactors tool
  //
  //

  m_trigEffSF_tool_name = "MuonTriggerScaleFactors_effSF_Trig_Reco" + m_WorkingPointRecoTrig + "_Iso" + m_WorkingPointIsoTrig;
  std::string trigEffSF_handle_name = "CP::MuonTriggerScaleFactors/" + m_trigEffSF_tool_name;

  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", checkToolStore<CP::MuonTriggerScaleFactors>(m_trigEffSF_tool_name), "" );
  std::string iso_trig_WP = "Iso" + m_WorkingPointIsoTrig;
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muTrigSF_tool_handle.makeNew<CP::MuonTriggerScaleFactors>(trigEffSF_handle_name), "Failed to create handle to CP::MuonTriggerScaleFactors for trigger efficiency SF");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muTrigSF_tool_handle.setProperty("Isolation", iso_trig_WP ),"Failed to set Isolation property of MuonTriggerScaleFactors");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muTrigSF_tool_handle.setProperty("MuonQuality", m_WorkingPointRecoTrig ),"Failed to set MuonQuality property of MuonTriggerScaleFactors");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muTrigSF_tool_handle.initialize(), "Failed to properly initialize CP::MuonTriggerScaleFactors for trigger efficiency SF");

  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", checkToolStore<CP::PileupReweightingTool>("Pileup"), "" );
  if ( m_isMC && ( !m_useRandomRunNumber || !isToolAlreadyUsed("Pileup") ) ) {
    Warning("initialize()","m_useRandomRunNumber is disabled OR couldn't find a configured CP::PileupReweightingTool in the asg::ToolStore!" );
    Warning("initialize()"," ===> setting runNumber %i read from user's configuration - NOT RECOMMENDED", m_runNumber );
    if ( m_muTrigSF_tool_handle->setRunNumber( m_runNumber ) == CP::CorrectionCode::Error ) { Warning("initialize()","Cannot set RunNumber for MuonTriggerScaleFactors tool"); }
  }

//  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_pileup_tool_handle.makeNew<CP::PileupReweightingTool>("CP::PileupReweightingTool/Pileup"), "Failed to create handle to CP::PileupReweightingTool");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_pileup_tool_handle.make("CP::PileupReweightingTool/Pileup"), "Failed to create handle to CP::PileupReweightingTool");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_pileup_tool_handle.initialize(), "Failed to properly initialize CP::PileupReweightingTool");

  //  Add the chosen WP to the string labelling the vector<SF>/vector<eff> decoration
  //
  m_outputSystNamesTrig      = m_outputSystNamesTrig + "_Reco" + m_WorkingPointRecoTrig + "_Iso" + m_WorkingPointIsoTrig;
  m_outputSystNamesTrigMCEff = m_outputSystNamesTrigMCEff + "_Reco" + m_WorkingPointRecoTrig + "_Iso" + m_WorkingPointIsoTrig;

  if ( m_debug ) {
    CP::SystematicSet affectSystsTrig = m_muTrigSF_tool_handle->affectingSystematics();
    for ( const auto& syst_it : affectSystsTrig ) { Info("initialize()","MuonEfficiencyScaleFactors tool can be affected by trigger efficiency systematic: %s", (syst_it.name()).c_str()); }
  }
  //
  // Make a list of systematics to be used, based on configuration input
  // Use HelperFunctions::getListofSystematics() for this!
  //
  const CP::SystematicSet recSystsTrig = m_muTrigSF_tool_handle->recommendedSystematics();
  m_systListTrig = HelperFunctions::getListofSystematics( recSystsTrig, m_systNameTrig, m_systValTrig, m_debug );

  Info("initialize()","Will be using MuonEfficiencyScaleFactors tool trigger efficiency systematic:");
  for ( const auto& syst_it : m_systListTrig ) {
    if ( m_systNameTrig.empty() ) {
      Info("initialize()","\t Running w/ nominal configuration only!");
      break;
    }
    Info("initialize()","\t %s", (syst_it.name()).c_str());
  }

  // 4.
  // initialize the CP::MuonEfficiencyScaleFactors Tool for track-to-vertex association (TTVA) SF
  //
  m_TTVAEffSF_tool_name = "MuonEfficiencyScaleFactors_effSF_" + m_WorkingPointTTVA;
  std::string TTVAEffSF_handle_name = "CP::MuonEfficiencyScaleFactors/" + m_TTVAEffSF_tool_name;

  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", checkToolStore<CP::MuonEfficiencyScaleFactors>(m_TTVAEffSF_tool_name), "" );
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muTTVASF_tool_handle.makeNew<CP::MuonEfficiencyScaleFactors>(TTVAEffSF_handle_name), "Failed to create handle to CP::MuonEfficiencyScaleFactors for TTVA efficiency SF");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muTTVASF_tool_handle.setProperty("WorkingPoint", m_WorkingPointTTVA ),"Failed to set Working Point property of MuonEfficiencyScaleFactors for TTVA efficiency SF");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muTTVASF_tool_handle.setProperty("CalibrationRelease", m_calibRelease ),"Failed to set calibration release property of MuonEfficiencyScaleFactors for TTVA efficiency SF");
  RETURN_CHECK("MuonEfficiencyCorrector::initialize()", m_muTTVASF_tool_handle.initialize(), "Failed to properly initialize CP::MuonEfficiencyScaleFactors for TTVA efficiency SF");

  //  Add the chosen WP to the string labelling the vector<SF> decoration
  //
  m_outputSystNamesTTVA = m_outputSystNamesTTVA + "_" + m_WorkingPointTTVA;

  if ( m_debug ) {
    CP::SystematicSet affectSystsTTVA = m_muTTVASF_tool_handle->affectingSystematics();
    for ( const auto& syst_it : affectSystsTTVA ) { Info("initialize()","MuonEfficiencyScaleFactors tool can be affected by TTVA efficiency systematic: %s", (syst_it.name()).c_str()); }
  }
  //
  // Make a list of systematics to be used, based on configuration input
  // Use HelperFunctions::getListofSystematics() for this!
  //
  const CP::SystematicSet recSystsTTVA = m_muTTVASF_tool_handle->recommendedSystematics();
  m_systListTTVA = HelperFunctions::getListofSystematics( recSystsTTVA, m_systNameTTVA, m_systValTTVA, m_debug );

  Info("initialize()","Will be using MuonEfficiencyScaleFactors tool TTVA efficiency systematic:");
  for ( const auto& syst_it : m_systListTTVA ) {
    if ( m_systNameTTVA.empty() ) {
      Info("initialize()","\t Running w/ nominal configuration only!");
      break;
    }
    Info("initialize()","\t %s", (syst_it.name()).c_str());
  }

  // *********************************************************************************

  Info("initialize()", "MuonEfficiencyCorrector Interface succesfully initialized!" );

  return EL::StatusCode::SUCCESS;
}


EL::StatusCode MuonEfficiencyCorrector :: execute ()
{
  // Here you do everything that needs to be done on every single
  // events, e.g. read input variables, apply cuts, and fill
  // histograms and trees.  This is where most of your actual analysis
  // code will go.

  m_numEvent++;

  if ( !m_isMC ) {
    if ( m_numEvent == 1 ) { Info("execute()", "Sample is Data! Do not apply any Muon Efficiency correction... "); }
    return EL::StatusCode::SUCCESS;
  }

  if ( m_debug ) { Info("execute()", "Applying Muon Efficiency corrections... "); }


  const xAOD::EventInfo* eventInfo(nullptr);
  RETURN_CHECK("MuonEfficiencyCorrector::execute()", HelperFunctions::retrieve(eventInfo, m_eventInfoContainerName, m_event, m_store, m_verbose) ,"");

  // initialise containers
  //
  const xAOD::MuonContainer* inputMuons(nullptr);

  // if m_inputAlgoSystNames = "" --> input comes from xAOD, or just running one collection,
  // then get the one collection and be done with it

  // Declare a counter, initialised to 0
  // For the systematically varied input containers, we won't store again the vector with efficiency systs in TStore ( it will be always the same!)
  //
  unsigned int countInputCont(0);

  if ( m_inputAlgoSystNames.empty() ) {

    RETURN_CHECK("MuonEfficiencyCorrector::execute()", HelperFunctions::retrieve(inputMuons, m_inContainerName, m_event, m_store, m_verbose) ,"");

   if ( m_debug ) { Info( "execute", "Number of muons: %i", static_cast<int>(inputMuons->size()) ); }

    // decorate muons w/ SF - there will be a decoration w/ different name for each syst!
    //
    this->executeSF( eventInfo, inputMuons, countInputCont );

  } else {
  // if m_inputAlgo = NOT EMPTY --> you are retrieving syst varied containers from an upstream algo. This is the case of calibrators: one different SC
  // for each calibration syst applied

	// get vector of string giving the syst names of the upstream algo m_inputAlgo (rememeber: 1st element is a blank string: nominal case!)
	//
        std::vector<std::string>* systNames(nullptr);
        RETURN_CHECK("MuonEfficiencyCorrector::execute()", HelperFunctions::retrieve(systNames, m_inputAlgoSystNames, 0, m_store, m_verbose) ,"");

    	// loop over systematic sets available
	//
    	for ( auto systName : *systNames ) {

           RETURN_CHECK("MuonEfficiencyCorrector::execute()", HelperFunctions::retrieve(inputMuons, m_inContainerName+systName, m_event, m_store, m_verbose) ,"");

    	   if ( m_debug ){
	     Info( "execute", "Number of muons: %i", static_cast<int>(inputMuons->size()) );
	     Info( "execute", "Input syst: %s", systName.c_str() );
	     unsigned int idx(0);
    	     for ( auto mu : *(inputMuons) ) {
    	       Info( "execute", "Input muon %i, pt = %.2f GeV ", idx, (mu->pt() * 1e-3) );
    	       ++idx;
    	     }
    	   }

	   // decorate muons w/ SF - there will be a decoration w/ different name for each syst!
	   //
           this->executeSF( eventInfo, inputMuons, countInputCont );

	   // increment counter
	   //
	   ++countInputCont;

    	} // close loop on systematic sets available from upstream algo

   }

  // look what we have in TStore
  //
  if ( m_verbose ) { m_store->print(); }

  return EL::StatusCode::SUCCESS;

}


EL::StatusCode MuonEfficiencyCorrector :: postExecute ()
{
  // Here you do everything that needs to be done after the main event
  // processing.  This is typically very rare, particularly in user
  // code.  It is mainly used in implementing the NTupleSvc.

  if ( m_debug ) { Info("postExecute()", "Calling postExecute"); }

  return EL::StatusCode::SUCCESS;
}



EL::StatusCode MuonEfficiencyCorrector :: finalize ()
{
  // This method is the mirror image of initialize(), meaning it gets
  // called after the last event has been processed on the worker node
  // and allows you to finish up any objects you created in
  // initialize() before they are written to disk.  This is actually
  // fairly rare, since this happens separately for each worker node.
  // Most of the time you want to do your post-processing on the
  // submission node after all your histogram outputs have been
  // merged.  This is different from histFinalize() in that it only
  // gets called on worker nodes that processed input events.

  Info("finalize()", "Deleting tool instances...");

  return EL::StatusCode::SUCCESS;
}


EL::StatusCode MuonEfficiencyCorrector :: histFinalize ()
{
  // This method is the mirror image of histInitialize(), meaning it
  // gets called after the last event has been processed on the worker
  // node and allows you to finish up any objects you created in
  // histInitialize() before they are written to disk.  This is
  // actually fairly rare, since this happens separately for each
  // worker node.  Most of the time you want to do your
  // post-processing on the submission node after all your histogram
  // outputs have been merged.  This is different from finalize() in
  // that it gets called on all worker nodes regardless of whether
  // they processed input events.

  Info("histFinalize()", "Calling histFinalize");
  RETURN_CHECK("xAH::Algorithm::algFinalize()", xAH::Algorithm::algFinalize(), "");
  return EL::StatusCode::SUCCESS;
}

EL::StatusCode MuonEfficiencyCorrector :: executeSF ( const xAOD::EventInfo* eventInfo, const xAOD::MuonContainer* inputMuons, unsigned int countSyst  )
{

  //
  // In the following, every muon gets decorated with 2 vector<double>'s (for reco/iso efficiency SFs),
  // and the event w/ 1 vector<double> (for trigger efficiency SFs)
  // Each vector contains the SFs, one SF for each syst (first component of each vector will be the nominal SF).
  //
  // Additionally, we create these vector<string> with the SF syst names, so that we know which component corresponds to.
  // ( there's a 1:1 correspondence with the vector<double> defined above )
  //
  // These vector<string> are eventually stored in TStore
  //
  std::vector< std::string >* sysVariationNamesReco  = new std::vector< std::string >;
  std::vector< std::string >* sysVariationNamesIso   = new std::vector< std::string >;
  std::vector< std::string >* sysVariationNamesTrig  = new std::vector< std::string >;
  std::vector< std::string >* sysVariationNamesTTVA  = new std::vector< std::string >;

  // 1.
  // Reco efficiency SFs - this is a per-MUON weight
  //
  // Firstly, loop over available systematics for this tool - remember: syst == EMPTY_STRING --> nominal
  // Every systematic will correspond to a different SF!
  //

  // Do it only if a tool with *this* name hasn't already been used
  //
  if ( !isToolAlreadyUsed(m_recoEffSF_tool_name) ) {

    for ( const auto& syst_it : m_systListReco ) {

      // Create the name of the SF weight to be recorded
      //   template:  SYSNAME_MuRecoEff_SF
      //
      std::string sfName = "MuRecoEff_SF_" + m_WorkingPointReco;;
      if ( !syst_it.name().empty() ) {
    	 std::string prepend = syst_it.name() + "_";
    	 sfName.insert( 0, prepend );
      }
      if ( m_debug ) { Info("executeSF()", "Muon reco efficiency SF sys name (to be recorded in xAOD::TStore) is: %s", sfName.c_str()); }
      sysVariationNamesReco->push_back(sfName);

      // apply syst
      //
      if ( m_muRecoSF_tool_handle->applySystematicVariation(syst_it) != CP::SystematicCode::Ok ) {
    	Error("executeSF()", "Failed to configure MuonEfficiencyScaleFactors for systematic %s", syst_it.name().c_str());
    	return EL::StatusCode::FAILURE;
      }
      if ( m_debug ) { Info("executeSF()", "Successfully applied systematic: %s", syst_it.name().c_str()); }

      // and now apply reco efficiency SF!
      //
      unsigned int idx(0);
      for ( auto mu_itr : *(inputMuons) ) {

    	 if ( m_debug ) { Info( "executeSF()", "Applying reco efficiency SF" ); }

    	 // a)
    	 // decorate directly the muon with reco efficiency (useful at all?), and the corresponding SF
    	 //
    	 if ( m_muRecoSF_tool_handle->applyRecoEfficiency( *mu_itr ) != CP::CorrectionCode::Ok ) {
    	   Warning( "executeSF()", "Problem in applyRecoEfficiency");
    	 }
    	 if ( m_muRecoSF_tool_handle->applyEfficiencyScaleFactor( *mu_itr ) != CP::CorrectionCode::Ok ) {
    	   Warning( "executeSF()", "Problem in applyEfficiencyScaleFactor");
    	 }

    	 // b)
    	 // obtain reco efficiency SF as a float (to be stored away separately)
    	 //
    	 //  If SF decoration vector doesn't exist, create it (will be done only for the 1st systematic for *this* muon)
    	 //
    	 SG::AuxElement::Decorator< std::vector<float> > sfVecReco( m_outputSystNamesReco );
    	 if ( !sfVecReco.isAvailable( *mu_itr ) ) {
  	   sfVecReco( *mu_itr ) = std::vector<float>();
    	 }

    	 float recoEffSF(1.0);
    	 if ( m_muRecoSF_tool_handle->getEfficiencyScaleFactor( *mu_itr, recoEffSF ) != CP::CorrectionCode::Ok ) {
    	   Warning( "executeSF()", "Problem in getEfficiencyScaleFactor");
    	   recoEffSF = 1.0;
    	 }
    	 //
    	 // Add it to decoration vector
    	 //
    	 sfVecReco( *mu_itr ).push_back( recoEffSF );

    	 if ( m_debug ) {
    	   Info( "executeSF()", "===>>>");
    	   Info( "executeSF()", " ");
  	   Info( "executeSF()", "Muon %i, pt = %.2f GeV ", idx, (mu_itr->pt() * 1e-3) );
  	   Info( "executeSF()", " ");
  	   Info( "executeSF()", "Reco eff. SF decoration: %s", m_outputSystNamesReco.c_str() );
  	   Info( "executeSF()", " ");
    	   Info( "executeSF()", "Systematic: %s", syst_it.name().c_str() );
    	   Info( "executeSF()", " ");
    	   Info( "executeSF()", "Reco efficiency:");
    	   Info( "executeSF()", "\t %f (from applyRecoEfficiency())", mu_itr->auxdataConst< float >( "Efficiency" ) );
    	   Info( "executeSF()", "and its SF:");
    	   Info( "executeSF()", "\t %f (from applyEfficiencyScaleFactor())", mu_itr->auxdataConst< float >( "EfficiencyScaleFactor" ) );
    	   Info( "executeSF()", "\t %f (from getEfficiencyScaleFactor())", recoEffSF );
    	   Info( "executeSF()", "--------------------------------------");
    	 }

    	 ++idx;

      } // close muon loop

    }  // close loop on reco efficiency SF systematics

    // Add list of systematics names to TStore
    //
    // NB: we need to make sure that this is not pushed more than once in TStore!
    // This will be the case when this executeSF() function gets called for every syst varied input container,
    // e.g. the different SC containers w/ calibration systematics upstream.
    //
    // Use the counter defined in execute() to check this is done only once per event
    //
    if ( countSyst == 0 ) { RETURN_CHECK( "MuonEfficiencyCorrector::executeSF()", m_store->record( sysVariationNamesReco, m_outputSystNamesReco), "Failed to record vector of systematic names for muon reco efficiency SF" ); }

  }

  // 2.
  // Isolation efficiency SFs - this is a per-MUON weight
  //
  // Firstly, loop over available systematics for this tool - remember: syst == EMPTY_STRING --> nominal
  // Every systematic will correspond to a different SF!
  //

  // Do it only if a tool with *this* name hasn't already been used
  //
  if ( !isToolAlreadyUsed(m_isoEffSF_tool_name) ) {

    for ( const auto& syst_it : m_systListIso ) {

      // Create the name of the SF weight to be recorded
      //   template:  SYSNAME_MuIsoEff_SF_WP
      //
      std::string sfName = "MuIsoEff_SF_" + m_WorkingPointIso;
      if ( !syst_it.name().empty() ) {
    	 std::string prepend = syst_it.name() + "_";
    	 sfName.insert( 0, prepend );
      }
      if ( m_debug ) { Info("executeSF()", "Muon iso efficiency SF sys name (to be recorded in xAOD::TStore) is: %s", sfName.c_str()); }
      sysVariationNamesIso->push_back(sfName);

      // apply syst
      //
      if ( m_muIsoSF_tool_handle->applySystematicVariation(syst_it) != CP::SystematicCode::Ok ) {
    	Error("executeSF()", "Failed to configure MuonEfficiencyScaleFactors for systematic %s", syst_it.name().c_str());
    	return EL::StatusCode::FAILURE;
      }
      if ( m_debug ) { Info("executeSF()", "Successfully applied systematic: %s", syst_it.name().c_str()); }

      // and now apply Iso efficiency SF!
      //
      unsigned int idx(0);
      for ( auto mu_itr : *(inputMuons) ) {

    	 if ( m_debug ) { Info( "executeSF()", "Applying iso efficiency SF" ); }

    	 // a)
    	 // decorate directly the muon with iso efficiency (useful at all?), and the corresponding SF
    	 //
    	 if ( m_muIsoSF_tool_handle->applyRecoEfficiency( *mu_itr ) != CP::CorrectionCode::Ok ) {
    	   Warning( "executeSF()", "Problem in applyIsoEfficiency");
    	 }
    	 if ( m_muIsoSF_tool_handle->applyEfficiencyScaleFactor( *mu_itr ) != CP::CorrectionCode::Ok ) {
    	   Warning( "executeSF()", "Problem in applyEfficiencyScaleFactor");
    	 }

    	 // b)
    	 // obtain iso efficiency SF as a float (to be stored away separately)
    	 //
    	 //  If SF decoration vector doesn't exist, create it (will be done only for the 1st systematic for *this* muon)
    	 //
    	 SG::AuxElement::Decorator< std::vector<float> > sfVecIso( m_outputSystNamesIso );
    	 if ( !sfVecIso.isAvailable( *mu_itr ) ) {
  	   sfVecIso( *mu_itr ) = std::vector<float>();
    	 }

    	 float IsoEffSF(1.0);
    	 if ( m_muIsoSF_tool_handle->getEfficiencyScaleFactor( *mu_itr, IsoEffSF ) != CP::CorrectionCode::Ok ) {
    	   Warning( "executeSF()", "Problem in getEfficiencyScaleFactor");
  	   IsoEffSF = 1.0;
    	 }
    	 //
    	 // Add it to decoration vector
    	 //
    	 sfVecIso( *mu_itr ).push_back(IsoEffSF);

    	 if ( m_debug ) {
    	   Info( "executeSF()", "===>>>");
    	   Info( "executeSF()", " ");
  	   Info( "executeSF()", "Muon %i, pt = %.2f GeV ", idx, (mu_itr->pt() * 1e-3) );
  	   Info( "executeSF()", " ");
  	   Info( "executeSF()", "Isolation SF decoration: %s", m_outputSystNamesIso.c_str() );
  	   Info( "executeSF()", " ");
    	   Info( "executeSF()", "Systematic: %s", syst_it.name().c_str() );
    	   Info( "executeSF()", " ");
    	   Info( "executeSF()", "Iso efficiency:");
    	   Info( "executeSF()", "\t %f (from applyIsoEfficiency())", mu_itr->auxdataConst< float >( "ISOEfficiency" ) );
    	   Info( "executeSF()", "and its SF:");
    	   Info( "executeSF()", "\t %f (from applyEfficiencyScaleFactor())", mu_itr->auxdataConst< float >( "ISOEfficiencyScaleFactor" ) );
    	   Info( "executeSF()", "\t %f (from getEfficiencyScaleFactor())", IsoEffSF );
    	   Info( "executeSF()", "--------------------------------------");
    	 }

    	 ++idx;

      } // close muon loop

    }  // close loop on isolation efficiency SF systematics

    // Add list of systematics names to TStore
    //
    // NB: we need to make sure that this is not pushed more than once in TStore!
    // This will be the case when this executeSF() function gets called for every syst varied input container,
    // e.g. the different SC containers w/ calibration systematics upstream.
    //
    // Use the counter defined in execute() to check this is done only once per event
    //
    if ( countSyst == 0 ) { RETURN_CHECK( "MuonEfficiencyCorrector::executeSF()", m_store->record( sysVariationNamesIso, m_outputSystNamesIso),   "Failed to record vector of systematic names for muon iso efficiency SF" ); }

  }

  // 3.
  // Trigger efficiency SF - this is in principle given by the MCP tool as a per-EVENT weight
  //
  // To allow more freedom to the user, we calculate it as a per-muon weight with a trick
  // We store also the MC efficiency per-muon

  // Firstly, loop over available systematics for this tool - remember: syst == EMPTY_STRING --> nominal
  // Every systematic will correspond to a different SF!
  //
  // NB: calculation of the event SF is up to the analyzer

  // Do it only if a tool with *this* name hasn't already been used
  //
  if ( !isToolAlreadyUsed(m_trigEffSF_tool_name) ) {

    // Unless specifically switched off by the user,
    // use the per-event random runNumber weighted by integrated luminosity got from CP::PileupReweightingTool::getRandomRunNumber()
    // Source:
    // https://twiki.cern.ch/twiki/bin/view/AtlasProtected/ExtendedPileupReweighting#Generating_PRW_config_files
    // https://twiki.cern.ch/twiki/bin/view/AtlasProtected/MCPAnalysisGuidelinesMC15#Muon_trigger_efficiency_scale_fa
    //
    if ( m_isMC && m_useRandomRunNumber ) {

      // Use mu-dependent randomization (recommended)
      //
      int randRunNumber = m_pileup_tool_handle->getRandomRunNumber( *eventInfo, true );

      int runNumber = ( randRunNumber != 0 ) ? randRunNumber : m_runNumber;

      if( m_muTrigSF_tool_handle->setRunNumber( runNumber ) == CP::CorrectionCode::Error ) {
    	Error("executeSF()", "Failed to set RunNumber for MuonTriggerScaleFactors tool");
    	return EL::StatusCode::FAILURE;
      }

    }

    for ( const auto& syst_it : m_systListIso ) {

      // Create the name of the SF weight to be recorded
      //   template:  SYSNAME_MuTrigEff_SF
      //
      std::string sfName = "MuTrigEff_SF_" + m_WorkingPointRecoTrig + "_" + m_WorkingPointIsoTrig;
      if ( !syst_it.name().empty() ) {
    	 std::string prepend = syst_it.name() + "_";
    	 sfName.insert( 0, prepend );
      }
      if ( m_debug ) { Info("executeSF()", "Trigger efficiency SF sys name (to be recorded in xAOD::TStore) is: %s", sfName.c_str()); }
      sysVariationNamesTrig->push_back(sfName);

      // apply syst
      //
      if ( m_muTrigSF_tool_handle->applySystematicVariation(syst_it) != CP::SystematicCode::Ok ) {
    	Error("executeSF()", "Failed to configure MuonTriggerScaleFactors for systematic %s", syst_it.name().c_str());
    	return EL::StatusCode::FAILURE;
      }
      if ( m_debug ) { Info("executeSF()", "Successfully applied systematic: %s", syst_it.name().c_str()); }

      // and now apply trigger efficiency SF!
      //
      unsigned int idx(0);
      for ( auto mu_itr : *(inputMuons) ) {

    	 if ( m_debug ) { Info( "executeSF()", "Applying trigger efficiency SF and MC efficiency" ); }

    	 // Pass a container with only the muon in question to the tool
    	 // (use a view container to be light weight)
    	 //
    	 ConstDataVector<xAOD::MuonContainer> mySingleMuonCont(SG::VIEW_ELEMENTS);
    	 mySingleMuonCont.push_back( mu_itr );

    	 //  If SF decoration vector doesn't exist, create it (will be done only for the 1st systematic for *this* muon)
    	 //
    	 SG::AuxElement::Decorator< std::vector<float> > sfVecTrig( m_outputSystNamesTrig );
    	 if ( !sfVecTrig.isAvailable( *mu_itr ) ) {
  	   sfVecTrig( *mu_itr ) = std::vector<float>();
    	 }
    	 SG::AuxElement::Decorator< std::vector<float> > effMC( m_outputSystNamesTrigMCEff );
    	 if ( !effMC.isAvailable( *mu_itr ) ) {
  	   effMC( *mu_itr ) = std::vector<float>();
    	 }

    	 double triggerEffSF(1.0); // tool wants a double
    	 if ( !m_SingleMuTrig.empty() && m_muTrigSF_tool_handle->getTriggerScaleFactor( *mySingleMuonCont.asDataVector(), triggerEffSF, m_SingleMuTrig ) != CP::CorrectionCode::Ok ) {
  	   Warning( "executeSF()", "Problem in getTriggerScaleFactor - single muon trigger(s)");
  	   triggerEffSF = 1.0;
    	 }

    	 // Add it to decoration vector
    	 //
    	 sfVecTrig( *mu_itr ).push_back(triggerEffSF);

    	 double triggerMCEff(0.0); // tool wants a double
    	 if ( !m_SingleMuTrig.empty() && m_muTrigSF_tool_handle->getTriggerEfficiency( *mu_itr, triggerMCEff, m_SingleMuTrig, !m_isMC ) != CP::CorrectionCode::Ok ) {
    	   Warning( "executeSF()", "Problem in getTriggerEfficiency - single muon trigger(s)");
    	   triggerMCEff = 0.0;
    	 }

    	 // Add it to decoration vector
    	 //
    	 effMC( *mu_itr ).push_back(triggerMCEff);

    	 if ( m_debug ) {
    	   Info( "executeSF()", "===>>>");
    	   Info( "executeSF()", " ");
  	   Info( "executeSF()", "Muon %i, pt = %.2f GeV ", idx, (mu_itr->pt() * 1e-3) );
  	   Info( "executeSF()", " ");
  	   Info( "executeSF()", "Trigger efficiency SF decoration: %s", m_outputSystNamesTrig.c_str() );
  	   Info( "executeSF()", "Trigger MC efficiency decoration: %s", m_outputSystNamesTrigMCEff.c_str() );
  	   Info( "executeSF()", " ");
    	   Info( "executeSF()", "Systematic: %s", syst_it.name().c_str() );
    	   Info( "executeSF()", " ");
    	   Info( "executeSF()", "Trigger efficiency SF:");
    	   Info( "executeSF()", "\t %f (from getTriggerScaleFactor())", triggerEffSF );
    	   Info( "executeSF()", "Trigger MC efficiency:");
    	   Info( "executeSF()", "\t %f (from getTriggerEfficiency())", triggerMCEff );
    	   Info( "executeSF()", "--------------------------------------");
    	 }

    	 ++idx;

      } // close muon loop

    }  // close loop on trigger efficiency SF systematics

    // Add list of systematics names to TStore
    //
    // NB: we need to make sure that this is not pushed more than once in TStore!
    // This will be the case when this executeSF() function gets called for every syst varied input container,
    // e.g. the different SC containers w/ calibration systematics upstream.
    //
    // Use the counter defined in execute() to check this is done only once per event
    //
    if ( countSyst == 0 ) { RETURN_CHECK( "MuonEfficiencyCorrector::executeSF()", m_store->record( sysVariationNamesTrig, m_outputSystNamesTrig), "Failed to record vector of systematic names for muon trigger efficiency  SF" ); }

  }

  // 4.
  // TTVA efficiency SFs - this is a per-MUON weight
  //
  // Firstly, loop over available systematics for this tool - remember: syst == EMPTY_STRING --> nominal
  // Every systematic will correspond to a different SF!
  //

  // Do it only if a tool with *this* name hasn't already been used
  //
  if ( !isToolAlreadyUsed(m_TTVAEffSF_tool_name) ) {

    for ( const auto& syst_it : m_systListTTVA ) {

      // Create the name of the SF weight to be recorded
      //   template:  SYSNAME_MuTTVAEff_SF_WP
      //
      std::string sfName = "MuTTVAEff_SF_" + m_WorkingPointTTVA;
      if ( !syst_it.name().empty() ) {
    	 std::string prepend = syst_it.name() + "_";
    	 sfName.insert( 0, prepend );
      }
      if ( m_debug ) { Info("executeSF()", "Muon iso efficiency SF sys name (to be recorded in xAOD::TStore) is: %s", sfName.c_str()); }
      sysVariationNamesTTVA->push_back(sfName);

      // apply syst
      //
      if ( m_muTTVASF_tool_handle->applySystematicVariation(syst_it) != CP::SystematicCode::Ok ) {
    	Error("executeSF()", "Failed to configure MuonEfficiencyScaleFactors for systematic %s", syst_it.name().c_str());
    	return EL::StatusCode::FAILURE;
      }
      if ( m_debug ) { Info("executeSF()", "Successfully applied systematic: %s", syst_it.name().c_str()); }

      // and now apply TTVA efficiency SF!
      //
      unsigned int idx(0);
      for ( auto mu_itr : *(inputMuons) ) {

    	 if ( m_debug ) { Info( "executeSF()", "Applying TTVA efficiency SF" ); }

    	 // a)
    	 // decorate directly the muon with TTVA efficiency (useful at all?), and the corresponding SF
    	 //
    	 if ( m_muTTVASF_tool_handle->applyRecoEfficiency( *mu_itr ) != CP::CorrectionCode::Ok ) {
    	   Warning( "executeSF()", "Problem in applyTTVAEfficiency");
    	 }
    	 if ( m_muTTVASF_tool_handle->applyEfficiencyScaleFactor( *mu_itr ) != CP::CorrectionCode::Ok ) {
    	   Warning( "executeSF()", "Problem in applyEfficiencyScaleFactor");
    	 }

    	 // b)
    	 // obtain TTVA efficiency SF as a float (to be stored away separately)
    	 //
    	 //  If SF decoration vector doesn't exist, create it (will be done only for the 1st systematic for *this* muon)
    	 //
    	 SG::AuxElement::Decorator< std::vector<float> > sfVecTTVA( m_outputSystNamesTTVA );
    	 if ( !sfVecTTVA.isAvailable( *mu_itr ) ) {
  	   sfVecTTVA( *mu_itr ) = std::vector<float>();
    	 }

    	 float TTVAEffSF(1.0);
    	 if ( m_muTTVASF_tool_handle->getEfficiencyScaleFactor( *mu_itr, TTVAEffSF ) != CP::CorrectionCode::Ok ) {
    	   Warning( "executeSF()", "Problem in getEfficiencyScaleFactor");
  	   TTVAEffSF = 1.0;
    	 }
    	 //
    	 // Add it to decoration vector
    	 //
    	 sfVecTTVA( *mu_itr ).push_back(TTVAEffSF);

    	 if ( m_debug ) {
    	   Info( "executeSF()", "===>>>");
    	   Info( "executeSF()", " ");
  	   Info( "executeSF()", "Muon %i, pt = %.2f GeV ", idx, (mu_itr->pt() * 1e-3) );
  	   Info( "executeSF()", " ");
  	   Info( "executeSF()", "TTVA SF decoration: %s", m_outputSystNamesTTVA.c_str() );
  	   Info( "executeSF()", " ");
    	   Info( "executeSF()", "Systematic: %s", syst_it.name().c_str() );
    	   Info( "executeSF()", " ");
    	   Info( "executeSF()", "TTVA efficiency SF:");
    	   Info( "executeSF()", "\t %f (from getEfficiencyScaleFactor())", TTVAEffSF );
    	   Info( "executeSF()", "--------------------------------------");
    	 }

    	 ++idx;

      } // close muon loop

    }  // close loop on TTVA efficiency SF systematics

    // Add list of systematics names to TStore
    //
    // NB: we need to make sure that this is not pushed more than once in TStore!
    // This will be the case when this executeSF() function gets called for every syst varied input container,
    // e.g. the different SC containers w/ calibration systematics upstream.
    //
    // Use the counter defined in execute() to check this is done only once per event
    //
    if ( countSyst == 0 ) { RETURN_CHECK( "MuonEfficiencyCorrector::executeSF()", m_store->record( sysVariationNamesTTVA, m_outputSystNamesTTVA), "Failed to record vector of systematic names for muon TTVA efficiency  SF" ); }

  }

  return EL::StatusCode::SUCCESS;
}
