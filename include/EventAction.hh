/// \file EventAction.hh
/// \brief Definition of the B1::EventAction class
///        1イベント中の素子ごとのエネルギー損失を集計し、イベント終了時にROOTへ書き出す

#ifndef B1EventAction_h
#define B1EventAction_h 1

#include "G4UserEventAction.hh"
#include "globals.hh"

#include <array>

namespace B1
{

class RunAction;

class EventAction : public G4UserEventAction
{
  public:
    EventAction(RunAction* runAction);
    ~EventAction() override = default;

    void BeginOfEventAction(const G4Event* event) override;
    void EndOfEventAction(const G4Event* event) override;

    // 素子IDは 面番号*64 + ix*8 + iy (0 〜 319)。DetectorConstruction のコピー番号と同じ
    static constexpr G4int kNPix = 8;
    static constexpr G4int kNFace = 5;
    static constexpr G4int kNPixelTotal = kNFace * kNPix * kNPix;

    void AddPixelEdep(G4int pixelID, G4double edep)
    {
      if (pixelID >= 0 && pixelID < kNPixelTotal) fPixelEdep[pixelID] += edep;
    }

  private:
    std::array<G4double, kNPixelTotal> fPixelEdep{};
};

}  // namespace B1

#endif
