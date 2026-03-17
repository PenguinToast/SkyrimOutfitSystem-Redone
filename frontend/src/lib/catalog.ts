export type GearKind = 'armor' | 'weapon'

export type GearEntry = {
  id: string
  name: string
  editorId: string
  plugin: string
  kind: GearKind
  category: string
  slot: string
  armorRating?: number
  damage?: number
  weight: number
  value: number
  keywords: string[]
}

export type OutfitEntry = {
  id: string
  name: string
  editorId: string
  plugin: string
  summary: string
  pieces: string[]
  tags: string[]
}

export type EquipmentCatalog = {
  source: string
  revision: string
  gear: GearEntry[]
  outfits: OutfitEntry[]
}

export const defaultCatalog: EquipmentCatalog = {
  source: 'Bundled preview catalog',
  revision: 'ui-prototype',
  gear: [
    {
      id: 'armor-ebonymail',
      name: 'Ebony Mail',
      editorId: 'DA01EbonyMail',
      plugin: 'Skyrim.esm',
      kind: 'armor',
      category: 'Heavy Armor',
      slot: 'Body',
      armorRating: 45,
      weight: 28,
      value: 3200,
      keywords: ['daedric artifact', 'body', 'heavy', 'stealth'],
    },
    {
      id: 'armor-nightingale',
      name: 'Nightingale Armor',
      editorId: 'NightingaleArmorCuirass',
      plugin: 'Skyrim.esm',
      kind: 'armor',
      category: 'Light Armor',
      slot: 'Body',
      armorRating: 34,
      weight: 12,
      value: 2100,
      keywords: ['thieves guild', 'body', 'light', 'stealth'],
    },
    {
      id: 'armor-travelcloak',
      name: 'Traveler Fur Mantle',
      editorId: 'SOSNGTravelerMantle01',
      plugin: 'WarmFashion.esp',
      kind: 'armor',
      category: 'Clothing',
      slot: 'Cloak',
      armorRating: 0,
      weight: 2,
      value: 240,
      keywords: ['cloak', 'travel', 'fur', 'vanity'],
    },
    {
      id: 'armor-glassgauntlets',
      name: 'Glass Gauntlets',
      editorId: 'ArmorGlassGauntlets',
      plugin: 'Skyrim.esm',
      kind: 'armor',
      category: 'Light Armor',
      slot: 'Hands',
      armorRating: 11,
      weight: 4,
      value: 450,
      keywords: ['hands', 'light', 'glass'],
    },
    {
      id: 'armor-bosmerboots',
      name: 'Bosmer Hunter Boots',
      editorId: 'SOSNGBosmerHunterBoots',
      plugin: 'BosmeriSwag.esp',
      kind: 'armor',
      category: 'Light Armor',
      slot: 'Feet',
      armorRating: 9,
      weight: 3,
      value: 380,
      keywords: ['boots', 'hunter', 'bosmer', 'feet'],
    },
    {
      id: 'armor-courtcirclet',
      name: 'Silver Court Circlet',
      editorId: 'SOSNGCourtCircletSilver',
      plugin: 'RegalThreads.esp',
      kind: 'armor',
      category: 'Clothing',
      slot: 'Head',
      armorRating: 0,
      weight: 1,
      value: 900,
      keywords: ['circlet', 'head', 'noble', 'social'],
    },
    {
      id: 'weapon-dawnbreaker',
      name: 'Dawnbreaker',
      editorId: 'DA09Dawnbreaker',
      plugin: 'Skyrim.esm',
      kind: 'weapon',
      category: 'Sword',
      slot: 'One-Handed',
      damage: 12,
      weight: 10,
      value: 740,
      keywords: ['sword', 'one-handed', 'artifact', 'fire'],
    },
    {
      id: 'weapon-dragonbonebow',
      name: 'Dragonbone Bow',
      editorId: 'DLC2DragonBoneBow',
      plugin: 'Dawnguard.esm',
      kind: 'weapon',
      category: 'Bow',
      slot: 'Two-Handed',
      damage: 20,
      weight: 20,
      value: 2725,
      keywords: ['bow', 'dragonbone', 'archery'],
    },
    {
      id: 'weapon-orsimerdagger',
      name: 'Orsimer Skinning Knife',
      editorId: 'SOSNGOrsimerKnife01',
      plugin: 'OrcStrongholds.esp',
      kind: 'weapon',
      category: 'Dagger',
      slot: 'One-Handed',
      damage: 8,
      weight: 2,
      value: 180,
      keywords: ['dagger', 'knife', 'orc', 'utility'],
    },
    {
      id: 'weapon-ritualstaff',
      name: 'Ritual Bone Staff',
      editorId: 'SOSNGRitualBoneStaff',
      plugin: 'Witchlight.esp',
      kind: 'weapon',
      category: 'Staff',
      slot: 'Two-Handed',
      damage: 0,
      weight: 8,
      value: 1250,
      keywords: ['staff', 'ritual', 'bone', 'magic'],
    },
    {
      id: 'weapon-steelwaraxe',
      name: 'Steel War Axe',
      editorId: 'WeaponSteelWarAxe',
      plugin: 'Skyrim.esm',
      kind: 'weapon',
      category: 'War Axe',
      slot: 'One-Handed',
      damage: 9,
      weight: 11,
      value: 55,
      keywords: ['axe', 'one-handed', 'steel'],
    },
    {
      id: 'weapon-nordicherosword',
      name: 'Nordic Hero Greatsword',
      editorId: 'DLC2NordicHeroGreatsword',
      plugin: 'Dragonborn.esm',
      kind: 'weapon',
      category: 'Greatsword',
      slot: 'Two-Handed',
      damage: 21,
      weight: 16,
      value: 985,
      keywords: ['greatsword', 'nordic', 'two-handed'],
    },
  ],
  outfits: [
    {
      id: 'outfit-roadrunner',
      name: 'Roadrunner Kit',
      editorId: 'SOSNGOutfitRoadrunner',
      plugin: 'SkyrimOutfitSystemNG.esp',
      summary: 'Lightweight travel set for cities, inns, and carriage routes.',
      pieces: ['Traveler Fur Mantle', 'Bosmer Hunter Boots', 'Silver Court Circlet'],
      tags: ['travel', 'light', 'city'],
    },
    {
      id: 'outfit-nightops',
      name: 'Night Ops',
      editorId: 'SOSNGOutfitNightOps',
      plugin: 'SkyrimOutfitSystemNG.esp',
      summary: 'Stealth-forward vanity kit built around dark leather silhouettes.',
      pieces: ['Nightingale Armor', 'Glass Gauntlets', 'Orsimer Skinning Knife'],
      tags: ['stealth', 'thief', 'light armor'],
    },
    {
      id: 'outfit-courtly',
      name: 'Courtly Presence',
      editorId: 'SOSNGOutfitCourtlyPresence',
      plugin: 'SkyrimOutfitSystemNG.esp',
      summary: 'Social outfit for dialogue scenes, guild meetings, and palace visits.',
      pieces: ['Silver Court Circlet', 'Traveler Fur Mantle'],
      tags: ['social', 'noble', 'city'],
    },
    {
      id: 'outfit-dwemerbreaker',
      name: 'Dwemer Breaker',
      editorId: 'SOSNGOutfitDwemerBreaker',
      plugin: 'SkyrimOutfitSystemNG.esp',
      summary: 'Heavy ruin-delving silhouette with practical gloves and a visible artifact chestpiece.',
      pieces: ['Ebony Mail', 'Glass Gauntlets', 'Steel War Axe'],
      tags: ['dungeon', 'heavy', 'dwemer'],
    },
    {
      id: 'outfit-dragonguard',
      name: 'Dragonguard Hunter',
      editorId: 'SOSNGOutfitDragonguardHunter',
      plugin: 'SkyrimOutfitSystemNG.esp',
      summary: 'Ranged-focused look with a dragonbone bow and traveler-layered profile.',
      pieces: ['Dragonbone Bow', 'Traveler Fur Mantle', 'Bosmer Hunter Boots'],
      tags: ['archery', 'hunter', 'field'],
    },
  ],
}
