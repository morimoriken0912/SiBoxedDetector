/// \file EventAction.cc
/// \brief Implementation of the B1::EventAction class

#include "EventAction.hh"

#include "G4AnalysisManager.hh"
#include "G4Event.hh"
#include "G4SystemOfUnits.hh"

namespace B1
{

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// ActionInitialization を変更しなくて済むよう、コンストラクタの引数は元のまま残している
EventAction::EventAction(RunAction*) {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EventAction::BeginOfEventAction(const G4Event*)
{
  fHitPixel.clear();
  fHitEdep.clear();
  fHitTime.clear();
  fHitPdg.clear();
  
  fNGenAlphas = 0;
  fNGenGammas = 0;
  fNGenElectrons = 0;
  fNGenHeavyIons = 0;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EventAction::EndOfEventAction(const G4Event* event)
{
  auto analysisManager = G4AnalysisManager::Instance();

  const G4int eventID = event->GetEventID();
  // ベクターに保存された独立したヒットをすべて書き出す
  G4int nHitPixels = fHitPixel.size(); // ヒットした総数
  G4double edepTotal = 0.;

  for (size_t i = 0; i < fHitPixel.size(); ++i) {
    edepTotal += fHitEdep[i];

    G4int face = fHitPixel[i] / 64;
    G4int ix   = (fHitPixel[i] % 64) / 8;
    G4int iy   = (fHitPixel[i] % 64) % 8;

    analysisManager->FillNtupleIColumn(1, 0, eventID);
    analysisManager->FillNtupleIColumn(1, 1, fHitPixel[i]);
    analysisManager->FillNtupleIColumn(1, 2, face);
    analysisManager->FillNtupleIColumn(1, 3, ix);
    analysisManager->FillNtupleIColumn(1, 4, iy);
    analysisManager->FillNtupleDColumn(1, 5, fHitEdep[i] / MeV);
    analysisManager->FillNtupleDColumn(1, 6, fHitTime[i] / microsecond);
    analysisManager->FillNtupleIColumn(1, 7, fHitPdg[i]);
    analysisManager->FillH1(1, fHitEdep[i]);
    analysisManager->AddNtupleRow(1);
  }

  // eventsツリーへの書き込み（変更なし）
  analysisManager->FillNtupleIColumn(0, 0, eventID);
  analysisManager->FillNtupleDColumn(0, 1, edepTotal / MeV);
  analysisManager->FillNtupleIColumn(0, 2, nHitPixels);
  analysisManager->FillNtupleIColumn(0, 3, fNGenAlphas);
  analysisManager->FillNtupleIColumn(0, 4, fNGenGammas);
  analysisManager->FillNtupleIColumn(0, 5, fNGenElectrons);
  analysisManager->FillNtupleIColumn(0, 6, fNGenHeavyIons);
  analysisManager->AddNtupleRow(0);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

}  // namespace B1
