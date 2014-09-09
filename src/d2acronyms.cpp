char const* statAcronyms[512] = {
  "$1 str", // strength
  "$1 ene", // energy
  "$1 dex", // dexterity
  "$1 vit", // vitality
  0, // statpts
  0, // newskills
  0, // hitpoints
  "$1 life", // maxhp
  0, // mana
  "$1 mana", // maxmana
  0, // stamina
  "$1 stamina", // maxstamina
  0, // level
  0, // experience
  0, // gold
  0, // goldbank
  "$1% def", // item_armor_percent
  0, // item_maxdamage_percent
  0, // item_mindamage_percent
  "$1 ar", // tohit
  "$1% block", // toblock
  "$1 min", // mindamage
  "$1 max", // maxdamage
  "$1 min", // secondary_mindamage
  "$1 max", // secondary_maxdamage
  0, // damagepercent
  0, // manarecovery
  "$1% mana reg", // manarecoverybonus
  "$1% stamina reg", // staminarecoverybonus
  0, // lastexp
  0, // nextexp
  "+$1 def", // armorclass
  "$1 missile def", // armorclass_vs_missile
  "$1 melee def", // armorclass_vs_hth
  "$1 dr", // normal_damage_reduction
  "$1 mdr", // magic_damage_reduction
  "$1% dr", // damageresist
  "$1% mdr", // magicresist
  "$1% max magic res", // maxmagicresist
  "$1 fr", // fireresist
  "$1 max fr", // maxfireresist
  "$1 lr", // lightresist
  "$1 max lr", // maxlightresist
  "$1 cr", // coldresist
  "$1 max cr", // maxcoldresist
  "$1 pr", // poisonresist
  "$1 max pr", // maxpoisonresist
  0, // damageaura
  "$1 min fire", // firemindam
  "$1 max fire", // firemaxdam
  "$1 min light", // lightmindam
  "$1 max light", // lightmaxdam
  "$1 min magic", // magicmindam
  "$1 max magic", // magicmaxdam
  "$1 min cold", // coldmindam
  "$1 max cold", // coldmaxdam
  0, // coldlength
  "$1 min psn", // poisonmindam
  "$1 max psn", // poisonmaxdam
  0, // poisonlength
  "$1 ll", // lifedrainmindam
  0, // lifedrainmaxdam
  "$1 ml", // manadrainmindam
  0, // manadrainmaxdam
  0, // stamdrainmindam
  0, // stamdrainmaxdam
  0, // stunlength
  0, // velocitypercent
  0, // attackrate
  0, // other_animrate
  0, // quantity
  0, // value
  0, // durability
  0, // maxdurability
  "$1 repl", // hpregen
  "$1% dura", // item_maxdurability_percent
  "$1% life", // item_maxhp_percent
  "$1% mana", // item_maxmana_percent
  "$1 thorns", // item_attackertakesdamage
  "$1% gold", // item_goldbonus
  "$1 mf", // item_magicbonus
  "kb", // item_knockback
  0, // item_timeduration
  "$1 $0", // item_addclassskills
  0, // unsentparam1
  "$1% exp", // item_addexperience
  "$1 laek", // item_healafterkill
  "$1% prices", // item_reducedprices
  0, // item_doubleherbduration
  "$1 light radius", // item_lightradius
  0, // item_lightcolor
  "$1% req", // item_req_percent
  0, // item_levelreq
  "$1 ias", // item_fasterattackrate
  0, // item_levelreqpct
  0, // lastblockframe
  "$1 frw", // item_fastermovevelocity
  "$1 $0", // item_nonclassskill
  0, // state
  "$1 fhr", // item_fastergethitrate
  0, // monster_playercount
  0, // skill_poison_override_length
  "$1% fbr", // item_fasterblockrate
  0, // skill_bypass_undead
  0, // skill_bypass_demons
  "$1 fcr", // item_fastercastrate
  0, // skill_bypass_beasts
  "$1 $0", // item_singleskill
  "rip", // item_restinpeace
  0, // curse_resistance
  "$1% plr", // item_poisonlengthresist
  "$1-$2 dmg", // item_normaldamage
  "$*flee", // item_howl
  "$*blinds", // item_stupidity
  "$1% dtm", // item_damagetomana
  "ignore def", // item_ignoretargetac
  "$1% target def", // item_fractionaltargetac
  "pmh", // item_preventheal
  "hfd", // item_halffreezeduration
  "$1% ar", // item_tohit_percent
  0, // item_damagetargetac
  0, // item_demondamage_percent
  0, // item_undeaddamage_percent
  0, // item_demon_tohit
  0, // item_undead_tohit
  0, // item_throwable
  "$1 $0", // item_elemskill
  "$1 skills", // item_allskills
  0, // item_attackertakeslightdamage
  0, // ironmaiden_level
  0, // lifetap_level
  0, // thorns_percent
  0, // bonearmor
  0, // bonearmormax
  "$*freeze", // item_freeze
  "$1% ow", // item_openwounds
  "$1% cb", // item_crushingblow
  0, // item_kickdamage
  "$1 maek", // item_manaafterkill
  0, // item_healafterdemonkill
  0, // item_extrablood
  "$1% crit", // item_deadlystrike
  "$1% fire abs", // item_absorbfire_percent
  "$1 fire abs", // item_absorbfire
  "$1% light abs", // item_absorblight_percent
  "$1 light abs", // item_absorblight
  "$1% magic abs", // item_absorbmagic_percent
  "$1 magic abs", // item_absorbmagic
  "$1% cold abs", // item_absorbcold_percent
  "$1 cold abs", // item_absorbcold
  "$1% slow", // item_slow
  "$1 aura", // item_aura
  "indestructible", // item_indesctructible
  "cbf", // item_cannotbefrozen
  0, // item_staminadrainpct
  0, // item_reanimate
  0, // item_pierce
  0, // item_magicarrow
  0, // item_explosivearrow
  0, // item_throw_mindamage
  0, // item_throw_maxdamage
  0, // skill_handofathena
  0, // skill_staminapercent
  0, // skill_passive_staminapercent
  0, // skill_concentration
  0, // skill_enchant
  0, // skill_pierce
  0, // skill_conviction
  0, // skill_chillingarmor
  0, // skill_frenzy
  0, // skill_decrepify
  0, // skill_armor_percent
  0, // alignment
  0, // target0
  0, // target1
  0, // goldlost
  0, // conversion_level
  0, // conversion_maxhp
  0, // unit_dooverlay
  0, // attack_vs_montype
  0, // damage_vs_montype
  0, // fade
  0, // armor_override_percent
  0, // unused183
  0, // unused184
  0, // unused185
  0, // unused186
  0, // unused187
  "$1 $0", // item_addskill_tab
  0, // unused189
  0, // unused190
  0, // unused191
  0, // unused192
  0, // unused193
  "$1 sox", // item_numsockets
  "$1% lvl $2 $0 on attack", // item_skillonattack
  "$1% lvl $2 $0 on kill", // item_skillonkill
  "$1% lvl $2 $0 on death", // item_skillondeath
  "$1% lvl $2 $0 on hit", // item_skillonhit
  "$1% lvl $2 $0 on lvlup", // item_skillonlevelup
  0, // unused200
  "$1% lvl $2 $0 on get hit", // item_skillongethit
  0, // unused202
  0, // unused203
  "$0 charges", // item_charged_skill
  0, // unused204
  0, // unused205
  0, // unused206
  0, // unused207
  0, // unused208
  0, // unused209
  0, // unused210
  0, // unused211
  0, // unused212
  "def/lvl", // item_armor_perlevel
  "def%/lvl", // item_armorpercent_perlevel
  "life/lvl", // item_hp_perlevel
  "mana/lvl", // item_mana_perlevel
  "dmg/lvl", // item_maxdamage_perlevel
  "dmg%/lvl", // item_maxdamage_percent_perlevel
  "str/lvl", // item_strength_perlevel
  "dex/lvl", // item_dexterity_perlevel
  "ene/lvl", // item_energy_perlevel
  "vit/lvl", // item_vitality_perlevel
  "fools", // item_tohit_perlevel
  "visio", // item_tohitpercent_perlevel
  "cold/lvl", // item_cold_damagemax_perlevel
  "fire/lvl", // item_fire_damagemax_perlevel
  "light/lvl", // item_ltng_damagemax_perlevel
  "psn/lvl", // item_pois_damagemax_perlevel
  "cr/lvl", // item_resist_cold_perlevel
  "fr/lvl", // item_resist_fire_perlevel
  "lr/lvl", // item_resist_ltng_perlevel
  "pr/lvl", // item_resist_pois_perlevel
  "coldabs/lvl", // item_absorb_cold_perlevel
  "fireabs/lvl", // item_absorb_fire_perlevel
  "lightabs/lvl", // item_absorb_ltng_perlevel
  "psnabs/lvl", // item_absorb_pois_perlevel
  "thorns/lvl", // item_thorns_perlevel
  "gold/lvl", // item_find_gold_perlevel
  "mf/lvl", // item_find_magic_perlevel
  0, // item_regenstamina_perlevel
  0, // item_stamina_perlevel
  0, // item_damage_demon_perlevel
  0, // item_damage_undead_perlevel
  0, // item_tohit_demon_perlevel
  0, // item_tohit_undead_perlevel
  "cb/lvl", // item_crushingblow_perlevel
  "ow/lvl", // item_openwounds_perlevel
  0, // item_kick_damage_perlevel
  "crit/lvl", // item_deadlystrike_perlevel
  0, // item_find_gems_perlevel
  "repl", // item_replenish_durability
  "repl", // item_replenish_quantity
  0, // item_extra_stack
  0, // item_find_item
  0, // item_slash_damage
  0, // item_slash_damage_percent
  0, // item_crush_damage
  0, // item_crush_damage_percent
  0, // item_thrust_damage
  0, // item_thrust_damage_percent
  0, // item_absorb_slash
  0, // item_absorb_crush
  0, // item_absorb_thrust
  0, // item_absorb_slash_percent
  0, // item_absorb_crush_percent
  0, // item_absorb_thrust_percent
  0, // item_armor_bytime
  0, // item_armorpercent_bytime
  0, // item_hp_bytime
  0, // item_mana_bytime
  0, // item_maxdamage_bytime
  0, // item_maxdamage_percent_bytime
  0, // item_strength_bytime
  0, // item_dexterity_bytime
  0, // item_energy_bytime
  0, // item_vitality_bytime
  0, // item_tohit_bytime
  0, // item_tohitpercent_bytime
  0, // item_cold_damagemax_bytime
  0, // item_fire_damagemax_bytime
  0, // item_ltng_damagemax_bytime
  0, // item_pois_damagemax_bytime
  0, // item_resist_cold_bytime
  0, // item_resist_fire_bytime
  0, // item_resist_ltng_bytime
  0, // item_resist_pois_bytime
  0, // item_absorb_cold_bytime
  0, // item_absorb_fire_bytime
  0, // item_absorb_ltng_bytime
  0, // item_absorb_pois_bytime
  0, // item_find_gold_bytime
  0, // item_find_magic_bytime
  0, // item_regenstamina_bytime
  0, // item_stamina_bytime
  0, // item_damage_demon_bytime
  0, // item_damage_undead_bytime
  0, // item_tohit_demon_bytime
  0, // item_tohit_undead_bytime
  0, // item_crushingblow_bytime
  0, // item_openwounds_bytime
  0, // item_kick_damage_bytime
  0, // item_deadlystrike_bytime
  0, // item_find_gems_bytime
  "$1% cold pierce", // item_pierce_cold
  "$1% fire pierce", // item_pierce_fire
  "$1% light pierce", // item_pierce_ltng
  "$1% psn pierce", // item_pierce_pois
  0, // item_damage_vs_monster
  0, // item_damage_percent_vs_monster
  0, // item_tohit_vs_monster
  0, // item_tohit_percent_vs_monster
  0, // item_ac_vs_monster
  0, // item_ac_percent_vs_monster
  0, // firelength
  0, // burningmin
  0, // burningmax
  0, // progressive_damage
  0, // progressive_steal
  0, // progressive_other
  0, // progressive_fire
  0, // progressive_cold
  0, // progressive_lightning
  0, // item_extra_charges
  0, // progressive_tohit
  0, // poison_count
  0, // damage_framerate
  0, // pierce_idx
  "$1% fire dmg", // passive_fire_mastery
  "$1% light dmg", // passive_ltng_mastery
  "$1% cold dmg", // passive_cold_mastery
  "$1% psn dmg", // passive_pois_mastery
  "$1% fire pierce", // passive_fire_pierce
  "$1% light pierce", // passive_ltng_pierce
  "$1% cold pierce", // passive_cold_pierce
  "$1% psn pierce", // passive_pois_pierce
  0, // passive_critical_strike
  0, // passive_dodge
  0, // passive_avoid
  0, // passive_evade
  0, // passive_warmth
  0, // passive_mastery_melee_th
  0, // passive_mastery_melee_dmg
  0, // passive_mastery_melee_crit
  0, // passive_mastery_throw_th
  0, // passive_mastery_throw_dmg
  0, // passive_mastery_throw_crit
  0, // passive_weaponblock
  0, // passive_summon_resist
  0, // modifierlist_skill
  0, // modifierlist_level
  0, // last_sent_hp_pct
  0, // source_unit_type
  0, // source_unit_id
  0, // shortparam1
  0, // questitemdifficulty
  0, // passive_mag_mastery
  0, // passive_mag_pierce

  "eth", // 359 = ethereal
  "$1 def", // total def

  "$1 stats", // all stats, 361
  "$1@", // all res, 362
  "$1-$2 dmg", // 363
  "$1-$2 fire dmg", // 364
  "$1-$2 cold dmg", // 365
  "$1-$2 light dmg", // 366
  "$1-$2 magic dmg", // 367
  "$1 psn dmg", // 368
  "$1% ed", // 369
};
