//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
/// \file DetectorConstruction.cc
/// \brief Implementation of the B1::DetectorConstruction class
///        Si pixel array box: 5 faces x (8 x 8 elements), open on the -z side (beam entrance),
///        with 3 mm plastic (PCB) plates on the outside of the 5 faces

#include "DetectorConstruction.hh"

#include "G4Box.hh"
#include "G4Colour.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4RotationMatrix.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Transform3D.hh"
#include "G4VisAttributes.hh"

namespace B1
{

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4VPhysicalVolume* DetectorConstruction::Construct()
{
  // Get nist material manager
  G4NistManager* nist = G4NistManager::Instance();

  // Envelope parameters
  //
  G4double env_sizeXY = 20 * cm, env_sizeZ = 30 * cm;
  // 元は水(G4_WATER)。Si検出器の手前でビームが減衰しないよう真空に変更
  G4Material* env_mat = nist->FindOrBuildMaterial("G4_Galactic");

  // Option to switch on/off checking of volumes overlaps
  //
  G4bool checkOverlaps = true;

  //
  // World
  //
  G4double world_sizeXY = 1.2 * env_sizeXY;
  G4double world_sizeZ = 1.2 * env_sizeZ;
  G4Material* world_mat = nist->FindOrBuildMaterial("G4_Galactic");

  auto solidWorld =
    new G4Box("World",  // its name
              0.5 * world_sizeXY, 0.5 * world_sizeXY, 0.5 * world_sizeZ);  // its size

  auto logicWorld = new G4LogicalVolume(solidWorld,  // its solid
                                        world_mat,  // its material
                                        "World");  // its name

  auto physWorld = new G4PVPlacement(nullptr,  // no rotation
                                     G4ThreeVector(),  // at (0,0,0)
                                     logicWorld,  // its logical volume
                                     "World",  // its name
                                     nullptr,  // its mother  volume
                                     false,  // no boolean operation
                                     0,  // copy number
                                     checkOverlaps);  // overlaps checking

  //
  // Envelope
  //
  auto solidEnv = new G4Box("Envelope",  // its name
                            0.5 * env_sizeXY, 0.5 * env_sizeXY, 0.5 * env_sizeZ);  // its size

  auto logicEnv = new G4LogicalVolume(solidEnv,  // its solid
                                      env_mat,  // its material
                                      "Envelope");  // its name

  new G4PVPlacement(nullptr,  // no rotation
                    G4ThreeVector(),  // at (0,0,0)
                    logicEnv,  // its logical volume
                    "Envelope",  // its name
                    logicWorld,  // its mother  volume
                    false,  // no boolean operation
                    0,  // copy number
                    checkOverlaps);  // overlaps checking

  //
  // Si pixel detector box
  //
  G4Material* si_mat = nist->FindOrBuildMaterial("G4_Si");

  const G4int nPix = 8;  // 1辺あたりの素子数
  const G4double pixSize = 7.5 * mm;  // 素子の1辺
  const G4double sensSize = 7.05 * mm;  // 有感部の1辺
  const G4double pixGap = 0.5 * mm;  // 素子間の隙間
  
  // ★ 不感層（Dead Layer）の設定
  const G4double pixThick = 150 * um;            // 素子の全体の厚さ
  const G4double deadThick = 0.177 * um;         // 表面の不感層の厚さ
  const G4double sensThick = pixThick - deadThick; // 有感部の厚さ (149.823 um)

  const G4double pixPitch = pixSize + pixGap;  // 素子のピッチ (= 8.0 mm)
  const G4double faceSize = nPix * pixPitch;  // 1面の大きさ (= 64 mm)
  const G4double halfFace = 0.5 * faceSize;  // BOX内側の半幅 (= 32 mm)
  const G4double wallOffset = halfFace + 0.5 * pixThick;  // 素子中心の位置 (原点から)

  // 素子全体 (薄い方向が z の Box)
  auto solidPixel = new G4Box("SiElement", 0.5 * pixSize, 0.5 * pixSize, 0.5 * pixThick);
  auto logicPixel = new G4LogicalVolume(solidPixel, si_mat, "SiElement");
  logicPixel->SetVisAttributes(new G4VisAttributes(G4Colour(0.4, 0.4, 0.8)));

  // 有感部 (厚さを sensThick に薄くする)
  auto solidSens = new G4Box("SiSensitive", 0.5 * sensSize, 0.5 * sensSize, 0.5 * sensThick);
  auto logicSens = new G4LogicalVolume(solidSens, si_mat, "SiSensitive");
  logicSens->SetVisAttributes(new G4VisAttributes(G4Colour(0.0, 0.8, 1.0)));

  // ★ 有感部を +z 方向にシフトさせ、-z 側（表面）に不感層を作る
  G4double zOffset = 0.5 * deadThick; 
  new G4PVPlacement(nullptr,  // no rotation
                    G4ThreeVector(0, 0, zOffset),  // 中心からズラす
                    logicSens,  // its logical volume
                    "SiSensitive",  // its name
                    logicPixel,  // its mother  volume
                    false,  // no boolean operation
                    0,  // copy number
                    checkOverlaps);  // overlaps checking

  // 面ごとの回転: 素子の「表面(不感層がある -z 側)」がすべて原点(入射側)を向くように設定
  G4RotationMatrix rotFace0; // +z 面 (回転なし)
  G4RotationMatrix rotFace1; rotFace1.rotateY(90. * deg);   // +x 面
  G4RotationMatrix rotFace2; rotFace2.rotateY(-90. * deg);  // -x 面
  G4RotationMatrix rotFace3; rotFace3.rotateX(-90. * deg);  // +y 面
  G4RotationMatrix rotFace4; rotFace4.rotateX(90. * deg);   // -y 面

  // face: 0=+z(奥), 1=+x, 2=-x, 3=+y, 4=-y
  for (G4int face = 0; face < 5; ++face) {
    for (G4int i = 0; i < nPix; ++i) {
      for (G4int j = 0; j < nPix; ++j) {
        // 面内での素子中心座標 (面の中心が原点)
        G4double u = (i - 0.5 * (nPix - 1)) * pixPitch;
        G4double v = (j - 0.5 * (nPix - 1)) * pixPitch;

        G4ThreeVector pos;
        G4RotationMatrix rot;
        switch (face) {
          case 0: pos = G4ThreeVector(u, v, +wallOffset); rot = rotFace0; break;
          case 1: pos = G4ThreeVector(+wallOffset, u, v); rot = rotFace1; break;
          case 2: pos = G4ThreeVector(-wallOffset, u, v); rot = rotFace2; break;
          case 3: pos = G4ThreeVector(u, +wallOffset, v); rot = rotFace3; break;
          case 4: pos = G4ThreeVector(u, -wallOffset, v); rot = rotFace4; break;
        }

        // コピー番号 = 面番号*64 + i*8 + j  (0 〜 319) で全素子を一意に識別できる
        G4int copyNo = face * nPix * nPix + i * nPix + j;

        new G4PVPlacement(G4Transform3D(rot, pos),  // 回転と位置
                          logicPixel,  // its logical volume
                          "SiElement",  // its name
                          logicEnv,  // its mother  volume
                          false,  // no boolean operation
                          copyNo,  // copy number
                          checkOverlaps);  // overlaps checking
      }
    }
  }

  //
  // PCB (ひとまず厚さ3 mmのプラスチック板)
  //
  G4Material* pcb_mat = nist->FindOrBuildMaterial("G4_POLYETHYLENE");  // 仮の材質
  const G4double pcbThick = 3. * mm;
  const G4double pcbOffset = halfFace + pixThick + 0.5 * pcbThick;  // Siの外側に接する位置

  auto colorPcb = new G4VisAttributes(G4Colour(0.0, 0.6, 0.2));

  // +z 面
  auto logicPcbZ = new G4LogicalVolume(new G4Box("PCB_Z", halfFace, halfFace, 0.5 * pcbThick),
                                       pcb_mat, "PCB_Z");
  logicPcbZ->SetVisAttributes(colorPcb);
  new G4PVPlacement(nullptr, G4ThreeVector(0, 0, +pcbOffset), logicPcbZ, "PCB_Z", logicEnv, false,
                    0, checkOverlaps);

  // ±x 面
  auto logicPcbX = new G4LogicalVolume(new G4Box("PCB_X", 0.5 * pcbThick, halfFace, halfFace),
                                       pcb_mat, "PCB_X");
  logicPcbX->SetVisAttributes(colorPcb);
  new G4PVPlacement(nullptr, G4ThreeVector(+pcbOffset, 0, 0), logicPcbX, "PCB_X", logicEnv, false,
                    0, checkOverlaps);
  new G4PVPlacement(nullptr, G4ThreeVector(-pcbOffset, 0, 0), logicPcbX, "PCB_X", logicEnv, false,
                    1, checkOverlaps);

  // ±y 面
  auto logicPcbY = new G4LogicalVolume(new G4Box("PCB_Y", halfFace, 0.5 * pcbThick, halfFace),
                                       pcb_mat, "PCB_Y");
  logicPcbY->SetVisAttributes(colorPcb);
  new G4PVPlacement(nullptr, G4ThreeVector(0, +pcbOffset, 0), logicPcbY, "PCB_Y", logicEnv, false,
                    0, checkOverlaps);
  new G4PVPlacement(nullptr, G4ThreeVector(0, -pcbOffset, 0), logicPcbY, "PCB_Y", logicEnv, false,
                    1, checkOverlaps);

  // Set Si sensitive part as scoring volume
  //
  fScoringVolume = logicSens;

  //
  // always return the physical World
  //
  return physWorld;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

}  // namespace B1