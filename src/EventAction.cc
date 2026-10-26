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
  fPixelEdep.fill(0.);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EventAction::EndOfEventAction(const G4Event* event)
{
  auto analysisManager = G4AnalysisManager::Instance();

  const G4int eventID = event->GetEventID();
  G4double edepTotal = 0.;
  G4int nHitPixels = 0;

  // ntuple 1 ("hits"): エネルギーを落とした素子ごとに1行
  for (G4int id = 0; id < kNPixelTotal; ++id) {
    const G4double edep = fPixelEdep[id];
    if (edep <= 0.) continue;

    ++nHitPixels;
    edepTotal += edep;

    const G4int face = id / (kNPix * kNPix);
    const G4int ix = (id % (kNPix * kNPix)) / kNPix;
    const G4int iy = id % kNPix;

    analysisManager->FillNtupleIColumn(1, 0, eventID);
    analysisManager->FillNtupleIColumn(1, 1, id);
    analysisManager->FillNtupleIColumn(1, 2, face);
    analysisManager->FillNtupleIColumn(1, 3, ix);
    analysisManager->FillNtupleIColumn(1, 4, iy);
    analysisManager->FillNtupleDColumn(1, 5, edep / MeV);
    analysisManager->FillH1(1, edep);  // hEdepPixel (MeV = 1 なので edep をそのまま詰める)
    analysisManager->AddNtupleRow(1);
  }

  // ヒストグラム hEdepTotal: 1イベントの合計エネルギー損失 (ヒットのあったイベントのみ)
  if (nHitPixels > 0) analysisManager->FillH1(0, edepTotal);

  // ntuple 0 ("events"): 全イベントについて1行 (ヒットなしのイベントも記録)
  analysisManager->FillNtupleIColumn(0, 0, eventID);
  analysisManager->FillNtupleDColumn(0, 1, edepTotal / MeV);
  analysisManager->FillNtupleIColumn(0, 2, nHitPixels);
  analysisManager->AddNtupleRow(0);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

}  // namespace B1
