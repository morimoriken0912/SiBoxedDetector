/// \file SteppingAction.cc
/// \brief Implementation of the B1::SteppingAction class

#include "SteppingAction.hh"

#include "DetectorConstruction.hh"
#include "EventAction.hh"

#include "G4LogicalVolume.hh"
#include "G4RunManager.hh"
#include "G4Step.hh"

namespace B1
{

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

SteppingAction::SteppingAction(EventAction* eventAction) : fEventAction(eventAction) {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void SteppingAction::UserSteppingAction(const G4Step* step)
{
  if (!fScoringVolume) {
    const auto detConstruction = static_cast<const DetectorConstruction*>(
      G4RunManager::GetRunManager()->GetUserDetectorConstruction());
    fScoringVolume = detConstruction->GetScoringVolume();
  }

  // get volume of the current step
  auto touchable = step->GetPreStepPoint()->GetTouchableHandle();
  G4LogicalVolume* volume = touchable->GetVolume()->GetLogicalVolume();

  // 有感部(SiSensitive)以外は無視
  if (volume != fScoringVolume) return;

  G4double edep = step->GetTotalEnergyDeposit();
  if (edep <= 0.) return;

  // 階層0 = SiSensitive、階層1 = SiElement。
  // SiElement のコピー番号が素子ID (面番号*64 + ix*8 + iy, 0 〜 319)
  G4int pixelID = touchable->GetCopyNumber(1);

  fEventAction->AddPixelEdep(pixelID, edep);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

}  // namespace B1
