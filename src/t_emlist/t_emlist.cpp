#include "types.h"

// Enemy list editor tool (D:/Bio4/Prog/t_emlist.cpp, module t_emlist): edits pG->emlist (the room's ESL
// enemy list) in place with a 3D cursor, saves/loads it through the host file system and re-creates the
// enemies with EmSetFromList. The per-enemy name tables come first: their strings open the object's
// .rodata, before the strings of the game headers included below.

// The original object's .data is 8-aligned (ours would be 4-aligned): the REL's .data starts 4 bytes
// after .rodata's end because of it.
asm(".section .data\n\t.balign 8\n\t.section .text");

// Name lists for the editor's bit / type / set fields, "END"-terminated (one per enemy id, shared where
// the enemies share an id family).
static const char* em02_sub_leon_flag[] = {"END"};
static const char* em02_sub_leon_type[] = {"END"};
static const char* em02_sub_leon_set[] = {"END"};
static const char* em03_sub_ashley_flag[] = {"", "END"};
static const char* em03_sub_ashley_set[] = {"END"};
static const char* em07_sub_test_flag[] = {"", "Louice", "Type A", "Type B", "END"};
static const char* em07_sub_test_set[] = {"Normal", "Corpse", "END"};
static const char* em0e_jetski_flag[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "END"
};
static const char* em0f_ship_flag[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "Light on", "END"
};
static const char* em0f_ship_type[] = {"R10B", "R10D", "R10E", "R10E SET", "R10E 2", "R10E 2 SET", "END"};
static const char* em0f_ship_set[] = {"Normal", "Ride", "END"};
static const char* em10_ganado_1_flag[] = {
    "MOVE START", "", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only", "Dash PL",
    "Hat2 on", "Hat on", "Roof on", "Weapon Torch", "", "Pickup waist", "", "", "Weapon Mugen",
    "Weapon Bomb", "knit2 Cap on", "Cap2 on", "Parasite on", "Glasses on", "knit Cap on", "Cap on",
    "Left Hand", "Run Start", "", "Weapon Sickle", "Weapon ChainSaw", "Weapon Axe", "Weapon Bucket",
    "Weapon Suki", "END"
};
static const char* em10_ganado_1_type[] = {
    "EM10", "----", "----", "EM15", "EM16", "----", "----", "----", "----", "----", "----", "EM11 Woman A",
    "----", "END"
};
static const char* em11_ganado_2_flag[] = {
    "MOVE START", "Hood Apron2", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only",
    "Dash PL", "Mask of Ram", "Mask of Gold", "Roof on", "Weapon Torch", "Hood 2", "Pickup waist",
    "Make up 2", "Weapon Bowgun", "Weapon Mugen", "Weapon Bomb", "Hood Apron", "Hood", "Parasite on",
    "Make up 1", "Gold Necklace", "Silver Necklace", "Left Hand", "Run Start", "Weapon Scythe", "", "",
    "Weapon M Star", "", "Shield", "END"
};
static const char* em11_ganado_2_type[] = {
    "----", "----", "----", "----", "----", "----", "----", "EM19 Evil A", "EM1A Evil B", "EM1B Evil C",
    "----", "----", "----", "END"
};
static const char* em12_ganado_1_c_flag[] = {
    "MOVE START", "", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only", "Dash PL",
    "Hat2 on", "Hat on", "Roof on", "Weapon Torch", "", "Pickup waist", "", "", "Weapon Mugen",
    "Weapon Bomb", "knit2 Cap on", "Cap2 on", "Parasite on", "Glasses on", "knit Cap on", "Cap on",
    "Left Hand", "Run Start", "", "Weapon Sickle", "", "Weapon Axe", "Weapon Bucket", "Weapon Suki", "END"
};
static const char* em12_ganado_1_c_type[] = {
    "EM10", "EM13 Young", "----", "EM15 G.papa", "EM16 C.S.", "----", "----", "----", "----", "----",
    "----", "----", "----", "END"
};
static const char* em13_ganado_1_w_flag[] = {
    "MOVE START", "", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only", "Dash PL",
    "Hat2 on", "Hat on", "Roof on", "Weapon Torch", "", "Pickup waist", "", "Weapon Bowgun",
    "Weapon Mugen", "Weapon Bomb", "knit2 Cap on", "Cap2 on", "Parasite on", "Glasses on", "knit Cap on",
    "Cap on", "Left Hand", "Run Start", "", "Weapon Sickle", "", "Weapon Axe", "Weapon Bucket",
    "Weapon Suki", "END"
};
static const char* em13_ganado_1_w_type[] = {
    "EM10", "EM13 Young", "----", "EM15 G.papa", "EM16 C.S.", "----", "EM18 Trader", "----", "----",
    "----", "----", "----", "----", "END"
};
static const char* em14_ganado_2_w_flag[] = {
    "MOVE START", "Hood Apron2", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only",
    "Dash PL", "Mask of Ram", "Mask of Gold", "Roof on", "Weapon Torch", "Hood 2", "Pickup waist",
    "Make up 2", "Weapon Bowgun", "Weapon Mugen", "Weapon Bomb", "Hood Apron", "Hood", "Parasite on",
    "Make up 1", "Gold Necklace", "Silver Necklace", "Left Hand", "Run Start", "Weapon Scythe", "", "",
    "Weapon M Star", "", "Shield", "END"
};
static const char* em14_ganado_2_w_type[] = {
    "----", "----", "----", "----", "----", "----", "EM18 Trader", "EM19 Evil A", "EM1A Evil B",
    "EM1B Evil C", "----", "----", "----", "END"
};
static const char* em15_ganado_1_c_flag[] = {
    "MOVE START", "", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only", "Dash PL",
    "Hat2 on", "Hat on", "Roof on", "Weapon Torch", "", "Pickup waist", "Weapon Knife", "", "Weapon Mugen",
    "Weapon Bomb", "Woman Hood2 on", "Cap2 on", "Parasite on", "Glasses on", "Woman Hood on", "Cap on",
    "Left Hand", "Run Start", "", "Weapon Sickle", "Weapon ChainSaw", "Weapon Axe", "Weapon Bucket",
    "Weapon Suki", "END"
};
static const char* em16_ganado_1_c2_flag[] = {
    "MOVE START", "", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only", "Dash PL",
    "Hat2 on", "Hat on", "Roof on", "Weapon Torch", "", "Pickup waist", "Weapon Knife", "", "Weapon Mugen",
    "Weapon Bomb", "Woman Hood2 on", "Cap2 on", "Parasite on", "Glasses on", "Woman Hood on", "Cap on",
    "Left Hand", "Run Start", "", "Weapon Sickle", "Weapon ChainSaw", "Weapon Axe", "", "Weapon Suki",
    "END"
};
static const char* em16_ganado_1_c2_type[] = {
    "----", "----", "----", "EM15", "EM16", "----", "----", "----", "----", "----", "----", "EM11 Woman A",
    "EM12 Woman B", "END"
};
static const char* em17_ganado_1_c2_flag[] = {
    "MOVE START", "", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only", "Dash PL",
    "Hat2 on", "Hat on", "Roof on", "Weapon Torch", "", "Pickup waist", "Weapon Knife", "", "Weapon Mugen",
    "Weapon Bomb", "Woman Hood2 on", "Cap2 on", "Parasite on", "Glasses on", "Woman Hood on", "Cap on",
    "Left Hand", "Run Start", "", "Weapon Sickle", "", "Weapon Axe", "", "Weapon Suki", "END"
};
static const char* em17_ganado_1_c2_type[] = {
    "EM10", "EM13 Young", "----", "EM15 G.papa", "----", "----", "----", "----", "----", "----", "----",
    "----", "EM12 Woman B", "END"
};
static const char* em19_ganado_2_a_flag[] = {
    "MOVE START", "Hood Apron2", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only",
    "Dash PL", "Mask of Ram", "Mask of Gold", "Roof on", "Weapon Torch", "Hood 2", "Pickup waist",
    "Make up 2", "Weapon Bowgun", "Weapon Mugen", "Weapon Bomb", "Hood Apron", "Hood", "Parasite on",
    "Make up 1", "Gold Necklace", "Silver Necklace", "Left Hand", "Run Start", "Weapon Scythe", "", "",
    "Weapon M Star", "", "Shield", "END"
};
static const char* em19_ganado_2_a_type[] = {
    "----", "----", "----", "----", "----", "----", "----", "EM19 Evil A", "EM1A Evil B", "EM1B Evil C",
    "----", "----", "----", "----", "END"
};
static const char* em1a_ganado_2_r_flag[] = {
    "MOVE START", "Hood Apron2", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only",
    "Dash PL", "Mask of Ram", "Mask of Gold", "Roof on", "Weapon Torch", "Hood 2", "Pickup waist",
    "Make up 2", "Weapon Bowgun", "Weapon Mugen", "Weapon Bomb", "Hood Apron", "Hood", "Parasite on",
    "Make up 1", "Gold Necklace", "Silver Necklace", "Left Hand", "Run Start", "Weapon Scythe", "", "",
    "Weapon M Star", "Weapon Rocket", "Shield", "END"
};
static const char* em1a_ganado_2_r_type[] = {
    "----", "----", "----", "----", "----", "----", "----", "EM19 Evil A", "EM1A Evil B", "EM1B Evil C",
    "----", "----", "----", "----", "END"
};
static const char* em1b_ganado_2_cm_flag[] = {
    "MOVE START", "Hood Apron2", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only",
    "Dash PL", "Mask of Ram", "Mask of Gold", "Roof on", "Weapon Torch", "Hood 2", "Pickup waist",
    "Make up 2", "Weapon Bowgun", "Weapon Mugen", "Weapon Bomb", "Hood Apron", "Hood", "Parasite on",
    "Make up 1", "Gold Necklace", "Silver Necklace", "Left Hand", "Run Start", "Weapon Scythe", "", "",
    "Weapon M Star", "", "Shield", "END"
};
static const char* em1b_ganado_2_cm_type[] = {
    "----", "----", "----", "----", "----", "----", "----", "EM19 Evil A", "EM1A Evil B", "----",
    "EM1C ClawMan", "----", "----", "----", "END"
};
static const char* em1c_ganado_2_cm2_type[] = {
    "----", "----", "----", "----", "----", "----", "----", "EM19 Evil A", "----", "EM1B Evil C",
    "EM1C ClawMan", "----", "----", "EM1D ClawMan 2", "END"
};
static const char* em1d_ganado_3_gm_flag[] = {
    "MOVE START", "Belt B", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only",
    "Dash PL", "Belt A", "Full Face Guard", "Roof on", "Weapon Club", "Mask C", "Pickup waist", "Mask B",
    "Weapon Bowgun", "Weapon Mugen", "Weapon Bomb", "Mask A", "Helmet B on", "Parasite on", "Helmet A on",
    "", "Cap on", "Left Hand", "Run Start", "Weapon StunRod", "Weapon Axe", "", "Weapon M Star",
    "Weapon Rocket", "Shield", "END"
};
static const char* em20_ganado_3_g_c_flag[] = {
    "MOVE START", "", "Low Power", "Lock PL", "Gunshot Musi", "Jump atk off", "Go Ashley only", "Dash PL",
    "Belt", "Full Face Guard", "Roof on", "Weapon Club", "Mask C", "Pickup waist", "Mask B",
    "Weapon Bowgun", "Weapon Mugen", "Weapon Bomb", "Mask A", "Helmet on", "Parasite on", "", "Belt C",
    "Cap on", "Left Hand", "Run Start", "Weapon StunRod", "Weapon Axe", "Weapon ChainSaw", "Weapon M Star",
    "Weapon Rocket", "Shield", "END"
};
static const char* em1d_ganado_3_gm_type[] = {
    "----", "----", "Gatling Man", "----", "----", "----", "----", "----", "----", "----", "----", "----",
    "----", "----", "EM1E Ganado A", "EM1E Ganado B", "EM1E Ganado C", "EM1E Ganado D", "EM1E Ganado A2",
    "EM1E Ganado B2", "EM1E Ganado C2", "EM1E Ganado D2", "----", "----", "----", "END"
};
static const char* em1e_ganado_3_w_type[] = {
    "----", "----", "----", "----", "----", "----", "EM18 Trader", "----", "----", "----", "----", "----",
    "----", "----", "EM1E Ganado A", "EM1E Ganado B", "----", "----", "----", "----", "----", "----",
    "----", "EM20 Gas Mask A", "----", "EM20 Gas Mask B", "END"
};
static const char* em1f_ganado_3_type[] = {
    "----", "----", "----", "----", "----", "----", "----", "----", "----", "----", "----", "----", "----",
    "----", "EM1E Ganado A", "EM1E Ganado B", "EM1E Ganado C", "----", "----", "----", "----", "----",
    "----", "----", "EM20 End of Century", "END"
};
static const char* em20_ganado_3_g_c_type[] = {
    "----", "----", "----", "----", "----", "----", "----", "----", "----", "----", "----", "----", "----",
    "----", "EM1E Ganado A", "EM1E Ganado B", "EM1E Ganado C", "EM1E Ganado D", "EM1E Ganado A2",
    "EM1E Ganado B2", "EM1E Ganado C2", "EM1E Ganado D2", "EM1F Chain Saw", "END"
};
static const char* em10_ganado_1_set[] = {
    "Normal", "----", "R100 Cliff A", "R100 Cliff B", "R100 Cliff C", "R101 Bucket A", "R101 Bucket B",
    "R101 Suki A", "R101 Suki B", "R101 Suki C", "R101 Sickle A", "R101 Cart A", "Evt:Dash start",
    "Evt:Walk start", "Walk", "Guard Left", "Guard Right", "Guard Sit", "----", "Hide", "Hide Fall",
    "Hide Jump", "Appear L", "Appear R", "Ride Truck", "Catapult mode", "R103 Bucket", "R100 Turn&Walk",
    "Rock Push", "R11C 1F IN", "R10C Parasite", "R100 Walk & Stay", "R202 Finger", "R11C 1F IN2",
    "Stay & Walk", "Attack wait", "R106 Fix Bomber", "Homing Bomber", "R204 Prayer", "R222 Dragon A",
    "R222 Dragon B", "R222 Dragon C", "R227 Barrel", "Homing Bomber2", "R10F G.Jump A", "R10F Gondola",
    "R209 Dash & Sit", "Evt:Wait Dash", "R10C ParaCancel", "R10F G.Jump B", "R11D Appear 1",
    "R11D Appear 2", "R212 Drill", "R201 Event wait", "Rocket Wait", "R21B Trolley Jump",
    "R21B Trolley Jump2", "R303 Fire Dash", "Work", "R300 Take Ashley", "R320 Gatling", "R300 Gatling",
    "R305 Bomber", "R321 Dead Body", "R408 Bomber", "END"
};
static const char* em10_ganado_1_chr[] = {"Chase", "Keep", "Rush", "Stop", "Escape", "In Room", "END"};
static const char* em18_trader_flag[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "END"
};
static const char* em18_trader_type[] = {"Normal", "Counter", "END"};
static const char* em18_trader_set[] = {"END"};
static const char* em3c_armor_flag[] = {
    "START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "END"
};
static const char* em3c_armor_type[] = {"Gray + AXE", "Gray + SWORD", "Black + AXE", "Black + SWORD", "END"};
static const char* em3c_armor_set[] = {"Normal", "Start Wait", "Atk Wait", "END"};
static const char* em29_bat_flag[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "END"
};
static const char* em29_bat_type[] = {"END"};
static const char* em29_bat_set[] = {"Land", "Ceiling", "END"};
static const char* em2a_trap_flag[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "END"
};
static const char* em2a_trap_type[] = {"Trap1 Hasami", "Trap2 P.E. Type A", "Trap2 P.E. Type B", "END"};
static const char* em2a_trap_set[] = {"Normal", "R100 Dog trap1", "Break", "END"};
static const char* em2b_elgigante_flag[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "Route around", "END"
};
static const char* em2b_elgigante_type[] = {"Type A", "Type Mask", "Type Handsome", "Type Blue", "END"};
static const char* em2b_elgigante_set[] = {"Normal", "From Event", "R11E Appear", "R224 Cage Wait", "END"};
static const char* em2c_insectboss_flag[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "END"
};
static const char* em2c_insectboss_type[] = {"Normal", "Tail", "END"};
static const char* em2c_insectboss_set[] = {"Normal", "Ceiling", "Re set", "Tail Hide", "END"};
static const char* em2d_insecthuman_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "No reset", "No wall", "No Camouflage", "END"
};
static const char* em2d_insecthuman_type[] = {
    "Normal", "Normal+Wing", "White", "White+Wing", "Simple+Wing", "Boss", "END"
};
static const char* em2d_insecthuman_set[] = {
    "Normal", "Air", "Ceiling", "Run start", "R213 Nest Wait", "Debug wall", "END"
};
static const char* em2e_spider_sml_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "END"
};
static const char* em2e_spider_sml_type[] = {"Normal", "White", "END"};
static const char* em2e_spider_sml_set[] = {"Normal", "Wall", "END"};
static const char* em2f_salamander_flag[] = {
    "MOVE START", "FIND ENEMY", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "END"
};
static const char* em2f_salamander_type[] = {"Type A", "Type B", "END"};
static const char* em2f_salamander_set[] = {"Wait", "Swim", "Critical", "END"};
static const char* em30_saddler_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "Parasite on", "END"
};
static const char* em30_saddler_type[] = {"Hood on", "Hood off", "END"};
static const char* em30_saddler_set[] = {"END"};
static const char* em31_saddler2_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "END"
};
static const char* em31_saddler2_type[] = {"Body", "Tentacle", "END"};
static const char* em31_saddler2_set[] = {"END"};
static const char* em32_u_3_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "Change Type3", "Change Type2", "END"
};
static const char* em32_u_3_type[] = {"END"};
static const char* em32_u_3_set[] = {"END"};
static const char* em34_no_1_no_2_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "END"
};
static const char* em34_no_1_no_2_type[] = {
    "No.1 Mayor", "No.2 Salazar", "Insect Boss 1", "Insect Boss 2", "END"
};
static const char* em34_no_1_no_2_set[] = {"END"};
static const char* em35_no_1_after_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "END"
};
static const char* em35_no_1_after_type[] = {"Normal", "Upper", "Lower", "END"};
static const char* em35_no_1_after_set[] = {"Normal", "Divide", "END"};
static const char* em36_regenerater_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "Weak Set", "Weak 5", "Weak 4", "Weak 3", "Weak 2", "Weak 1", "END"
};
static const char* em36_regenerater_type[] = {"Normal A", "Normal B", "Strong A", "Strong B", "END"};
static const char* em36_regenerater_set[] = {
    "Normal", "R307 Bed", "R309 Appear", "R308 Appear", "R310 Appear", "END"
};
static const char* em38_no_2_after_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "END"
};
static const char* em38_no_2_after_type[] = {"Body", "Tentacle A", "Tentacle B", "Upper", "Lower", "END"};
static const char* em38_no_2_after_set[] = {"END"};
static const char* em39_no_3_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "END"
};
static const char* em39_no_3_type[] = {"Type 1", "Type 2", "Type 3", "END"};
static const char* em39_no_3_set[] = {"Normal", "Test", "Success", "Failure", "Normal2", "END"};
static const char* em3a_seeker_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "END"
};
static const char* em3a_seeker_type[] = {"Machine Gun", "Missile", "Bomb Robbot", "END"};
static const char* em3a_seeker_set[] = {
    "Normal", "Up", "Down", "Right", "Left", "Forward", "Bomb Ground", "Hide2", "END"
};
static const char* em3b_truck_cart_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "END"
};
static const char* em3b_truck_cart_type[] = {"Truck", "Cart", "Stop Cart", "END"};
static const char* em3b_truck_cart_set[] = {"END"};
static const char* em21_dog_flag[] = {"END"};
static const char* em21_dog_type[] = {"TYPE0", "TYPE1", "END"};
static const char* em21_dog_set[] = {"Normal", "R100 Trap", "VS Elgigante", "END"};
static const char* em22_enemy_dog_flag[] = {
    "MOVE START", "FIND ENEMY", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "Parasite on", "No around", "END"
};
static const char* em22_enemy_dog_type[] = {"END"};
static const char* em22_enemy_dog_set[] = {
    "Normal", "R11B Set A", "R11B Set B", "R11B Set C", "In Cage", "Jump Wait", "END"
};
static const char* em23_crow_flag[] = {"END"};
static const char* em23_crow_type[] = {"END"};
static const char* em23_crow_set[] = {"Normal", "R20A Landing", "END"};
static const char* em24_snake_s_flag[] = {"END"};
static const char* em24_snake_s_type[] = {"END"};
static const char* em24_snake_s_set[] = {"Box wait", "Coil wait", "END"};
static const char* em25_parasite_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "END"
};
static const char* em25_parasite_type[] = {"END"};
static const char* em25_parasite_set[] = {"Hide", "Wait", "END"};
static const char* em26_cow_flag[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "END"
};
static const char* em26_cow_type[] = {"Type1", "Type2", "END"};
static const char* em26_cow_set[] = {"END"};
static const char* em27_blackbass_flag[] = {
    "MOVE START", "FIND ENEMY", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "END"
};
static const char* em27_blackbass_type[] = {"Normal", "Super Black Bass", "END"};
static const char* em27_blackbass_set[] = {"END"};
static const char* em28_chicken_flag[] = {
    "MOVE START", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "No Egg", "END"
};
static const char* em28_chicken_type[] = {"Type 1", "Type 2", "END"};
static const char* em28_chicken_set[] = {"END"};

// Per enemy id: display name and the name lists of its em flags, type, set and character fields.
struct EmListIdInfo {
    char name[16];
    const char** flag;
    const char** type;
    const char** set;
    const char** chr;
};

static EmListIdInfo EmListIdTbl[64] = {
    {"Player 0", NULL, NULL, NULL, NULL},
    {"Sub ", NULL, NULL, NULL, NULL},
    {"Sub Leon", em02_sub_leon_flag, em02_sub_leon_type, em02_sub_leon_set, NULL},
    {"Sub Ashley", em03_sub_ashley_flag, em03_sub_ashley_flag + 1, em03_sub_ashley_set, NULL},
    {"Sub Luis", NULL, NULL, NULL, NULL},
    {"Sub 5", NULL, NULL, NULL, NULL},
    {"Sub 6", NULL, NULL, NULL, NULL},
    {"Sub test", em07_sub_test_flag, em07_sub_test_flag + 1, em07_sub_test_set, NULL},
    {"Sub 8", NULL, NULL, NULL, NULL},
    {"Sub 9", NULL, NULL, NULL, NULL},
    {"Sub a", NULL, NULL, NULL, NULL},
    {"Sub b", NULL, NULL, NULL, NULL},
    {"Sub c", NULL, NULL, NULL, NULL},
    {"Sub d", NULL, NULL, NULL, NULL},
    {"JetSki", em0e_jetski_flag, NULL, NULL, NULL},
    {"Ship", em0f_ship_flag, em0f_ship_type, em0f_ship_set, NULL},
    {"Ganado 1", em10_ganado_1_flag, em10_ganado_1_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 2", em11_ganado_2_flag, em11_ganado_2_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 1-c", em12_ganado_1_c_flag, em12_ganado_1_c_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 1+w", em13_ganado_1_w_flag, em13_ganado_1_w_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 2+w", em14_ganado_2_w_flag, em14_ganado_2_w_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 1+c", em15_ganado_1_c_flag, em10_ganado_1_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 1+c2", em16_ganado_1_c2_flag, em16_ganado_1_c2_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 1-c2", em17_ganado_1_c2_flag, em17_ganado_1_c2_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Trader", em18_trader_flag, em18_trader_type, em18_trader_set, NULL},
    {"Ganado 2+A", em19_ganado_2_a_flag, em19_ganado_2_a_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 2+R", em1a_ganado_2_r_flag, em1a_ganado_2_r_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 2+cm", em1b_ganado_2_cm_flag, em1b_ganado_2_cm_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 2+cm2", em1b_ganado_2_cm_flag, em1c_ganado_2_cm2_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 3+GM", em1d_ganado_3_gm_flag, em1d_ganado_3_gm_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 3+W", em1d_ganado_3_gm_flag, em1e_ganado_3_w_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 3", em1d_ganado_3_gm_flag, em1f_ganado_3_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Ganado 3+G+C", em20_ganado_3_g_c_flag, em20_ganado_3_g_c_type, em10_ganado_1_set, em10_ganado_1_chr},
    {"Dog", em21_dog_flag, em21_dog_type, em21_dog_set, NULL},
    {"Enemy Dog", em22_enemy_dog_flag, em22_enemy_dog_type, em22_enemy_dog_set, NULL},
    {"Crow", em23_crow_flag, em23_crow_type, em23_crow_set, NULL},
    {"Snake S", em24_snake_s_flag, em24_snake_s_type, em24_snake_s_set, NULL},
    {"Parasite", em25_parasite_flag, em25_parasite_type, em25_parasite_set, NULL},
    {"Cow", em26_cow_flag, em26_cow_type, em26_cow_set, NULL},
    {"BlackBass", em27_blackbass_flag, em27_blackbass_type, em27_blackbass_set, NULL},
    {"Chicken", em28_chicken_flag, em28_chicken_type, em28_chicken_set, NULL},
    {"Bat", em29_bat_flag, em29_bat_type, em29_bat_set, NULL},
    {"Trap", em2a_trap_flag, em2a_trap_type, em2a_trap_set, NULL},
    {"Elgigante", em2b_elgigante_flag, em2b_elgigante_type, em2b_elgigante_set, NULL},
    {"InsectBoss", em2c_insectboss_flag, em2c_insectboss_type, em2c_insectboss_set, NULL},
    {"InsectHuman", em2d_insecthuman_flag, em2d_insecthuman_type, em2d_insecthuman_set, NULL},
    {"Spider sml", em2e_spider_sml_flag, em2e_spider_sml_type, em2e_spider_sml_set, NULL},
    {"Salamander", em2f_salamander_flag, em2f_salamander_type, em2f_salamander_set, NULL},
    {"Saddler", em30_saddler_flag, em30_saddler_type, em30_saddler_set, NULL},
    {"Saddler2", em31_saddler2_flag, em31_saddler2_type, em31_saddler2_set, NULL},
    {"U 3", em32_u_3_flag, em32_u_3_type, em32_u_3_set, NULL},
    {"------", NULL, NULL, NULL, NULL},
    {"No.1 & No.2", em34_no_1_no_2_flag, em34_no_1_no_2_type, em34_no_1_no_2_set, NULL},
    {"No.1 After", em35_no_1_after_flag, em35_no_1_after_type, em35_no_1_after_set, NULL},
    {"Regenerater", em36_regenerater_flag, em36_regenerater_type, em36_regenerater_set, NULL},
    {"------", NULL, NULL, NULL, NULL},
    {"No.2 After", em38_no_2_after_flag, em38_no_2_after_type, em38_no_2_after_set, NULL},
    {"No.3", em39_no_3_flag, em39_no_3_type, em39_no_3_set, NULL},
    {"Seeker", em3a_seeker_flag, em3a_seeker_type, em3a_seeker_set, NULL},
    {"Truck&Cart", em3b_truck_cart_flag, em3b_truck_cart_type, em3b_truck_cart_set, NULL},
    {"Armor", em3c_armor_flag, em3c_armor_type, em3c_armor_set, NULL},
    {"Helicopter", NULL, NULL, NULL, NULL},
    {"r22c Mark", NULL, NULL, NULL, NULL},
    {"", NULL, NULL, NULL, NULL},
};

#include "light.h"
#include "atari.h"
#include "em.h"
#include "em_set.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "main_sub.h"
#include "main_mem.h"
#include "scheduler.h"
#include "camera.h"
#include "file.h"
#include "t_prim.h"
#include "t_util.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "db_cam.h"
#include "cam_ctrl.h"
#include <stdio.h>
#include <string.h>

// The player's position is all the tool needs from pPL (player.h would add its header strings).
extern cEm* pPLem asm("pPL");
// The list index is stored through a reference in emlist_r0_target: the store then keeps the
// following `pG` loads in the search loops (a plain member store lets them hoist).

// The editor's view of a list entry (em_set.h EmListData with signed hp / Guard_r: the tool prints them
// with lha).
struct EmListEnt {
    u8 flags;       // 0x00
    u8 id;          // 0x01
    u8 type;        // 0x02
    u8 set;         // 0x03  (PS2 EM_LIST.set)
    u32 flags4;     // 0x04
    s16 hp;         // 0x08
    u8 emset_no;    // 0x0A  ("EmSet"; PS2 EM_LIST.emset_no)
    u8 Character;   // 0x0B  (PS2 EM_LIST.Character)
    u16 pos[3];     // 0x0C  (the editor steps them as unsigned halves)
    u16 rot[3];     // 0x12
    u16 room;       // 0x18  stage << 8 | room
    s16 Guard_r;    // 0x1A  * 1000 (PS2 EM_LIST.Guard_r)
    u8 pad_1C[4];
};

#define EMLIST_ENT(no) ((EmListEnt*) &pG->Em_list[no])
// The insert/paste searches address the entries tool-style (pG first in the add, shift index).
#define EMLIST_ENT_I(no) ((EmListEnt*) ((u32) pG + ((no) << 5) + PG_OFS(Em_list)))
// Entry-to-entry copies are byte-pointer memcpys: the stores then alias pG, which is reloaded per iteration.
#define EMLIST_COPY(dst, src) memcpy((u8*) pG + PG_OFS(Em_list) + (dst) * 0x20, (u8*) pG + PG_OFS(Em_list) + (src) * 0x20, 0x20)

// Editor state (0x3E0 bytes, Debug_alloc'd by emlist_init).
struct EmListWork {
    int routine;       // 0x00  index into EmList.routine
    int step;          // 0x04
    int x8;            // 0x08
    int xC;            // 0x0C
    int listNo;        // 0x10  entry of pG->emlist being edited
    int menuNo;        // 0x14  main menu cursor (target_menu / main_menu row)
    int subNo;         // 0x18  sub cursor (room byte / axis / bit)
    int fileSel;       // 0x1C  save/load file cursor (0 = cancel, 1..30 = file)
    int fileNo;        // 0x20  file number + 1 of the last save/load
    int yesNo;         // 0x24  yes/no confirm toggle (emlist_yes_no_menu_disp)
    EmListEnt copy;    // 0x28  copy buffer (Y copy / L+X overwrite / L+R+X paste)
    f32 cursorX;       // 0x48  screen cursor
    f32 cursorY;       // 0x4C
    int catchNo;       // 0x50  entry caught under the cursor (emlist_catch_em), -1 = none
    u8 id;             // 0x54  enemy id selected in the id menu
    u8 idNum;          // 0x55  number of named ids in EmListIdTbl
    u8 pad_56[6];
    EmListEnt cur;     // 0x5C  entry being edited
    int camMode;       // 0x7C  free camera mode (START)
    Camera cam;        // 0x80  free camera (emlistCamToPoin aims it at the entry)
    JOY joy;           // 0x178
};

static void emlist_r0_main();
static void emlist_r0_target();
static void emlist_r0_set_id();
static void emlist_r0_set_room();
static void emlist_r0_set_pos();
static void emlist_r0_set_ang();
static void emlist_r0_set_be_flag();
static void emlist_r0_set_type();
static void emlist_r0_set_set();
static void emlist_r0_set_em_flag();
static void emlist_r0_set_char();
static void emlist_r0_set_hp();
static void emlist_r0_set_guard_r();
static void emlist_r0_menu();
static void emlist_r0_save();
static void emlist_r0_load();
static void emlist_r0_clear();
static void emlist_r0_sort();
static void emlist_r0_set_exit();

// The original object carries 0x34 bytes of uninitialised static data members, which g++ 2.95 emits
// as COMMON (snmakerel appends them behind .bss; nothing references them, so their names and the
// header that defined them are unknown — probably `Mtx IDSystem::m_scrn_mat` (0x30, the block every
// em module has) plus one word). Reproduced as one COMMON object of that size.
struct EmListCommon {
    static u8 block[0x34];
};
u8 EmListCommon::block[0x34];

// The work pointer is a struct member: every store through it reloads the pointer.
struct EmListCtrl {
    EmListWork* wk;
};

static EmListCtrl EmList = {NULL};

// Routines by EmListWork::routine: 0 main (list cursor), 1 target (one entry's fields), 2..12 the
// field editors (id, room, pos, ang, be_flag, type, set, em flag, chara, hp, guard radius),
// 13 menu, 14 save, 15 load, 16 clear, 17 sort, 18 set and exit.
static void (*emlist_routine[19])() = {
    emlist_r0_main,
    emlist_r0_target,
    emlist_r0_set_id,
    emlist_r0_set_room,
    emlist_r0_set_pos,
    emlist_r0_set_ang,
    emlist_r0_set_be_flag,
    emlist_r0_set_type,
    emlist_r0_set_set,
    emlist_r0_set_em_flag,
    emlist_r0_set_char,
    emlist_r0_set_hp,
    emlist_r0_set_guard_r,
    emlist_r0_menu,
    emlist_r0_save,
    emlist_r0_load,
    emlist_r0_clear,
    emlist_r0_sort,
    emlist_r0_set_exit,
};

static char main_menu[7][0x80] = {
    "LIST      ",
    "SORT  LIST      ",
    "CLEAR LIST     ",
    "SET AND EXIT   (Don't set already set)",
    "LOAD      ",
    "SAVE      ",
    "EXIT      ",
};

static char target_menu[13][0x10] = {
    "ID      ", "ROOM    ", "POS     ", "ANG     ", "BE_FLAG ", "TYPE    ", "SET     ",
    "EM FLAG ", "CHARA   ", "HP      ", "Guard R ", "Set and Exit", "EXIT    ",
};

static char be_flag_name[8][0x10] = {"DIE", "", "", "", "", "", "SET", "ALIVE"};

static GXColor list_color[2] = {{0x80, 0x00, 0x00, 0xFF}, {0xFF, 0x40, 0x40, 0xFF}};

void emlist_init();
void emlist_exit();
void emlist_select_id();
void emlist_main_disp();
void emlist_target_help_disp();
void emlist_target_disp(int flag);
void emlist_target_menu_disp(int flag);
void emlist_menu_disp();
void emlist_file_menu_disp();
void emlist_yes_no_menu_disp(int y);
void emlist_select_id_disp();
void emlist_set_room_disp(int x, int y, int flag);
void emlist_set_pos_disp(int x, int y, int flag);
void emlist_set_ang_disp(int x, int y, int flag);
void emlist_set_be_flag_disp(int x, int y, int flag);
void emlist_set_type_disp(int x, int y, int flag);
void emlist_set_set_disp(int x, int y, int flag);
void emlist_set_em_flag_disp(int x, int y, int flag);
void emlist_set_char_disp(int x, int y, int flag);
void emlist_set_hp_disp(int x, int y, int flag);
void emlist_set_guard_r_disp(int x, int y, int flag);
void emlist_file_save(int no);
int emlist_file_load(int no);
void emlist_set_fname(char* buf, int no, int mode);
void emlist_EmDir_disp();
int emlist_catch_em();
// The original prototype has no parameter but the body reads the entry pointer from r3 (the
// caller leaves it there); the definition takes it explicitly under the original's mangled name.
extern "C" void emlist_em_move_to_cursor__Fv(EmListEnt* p);
int emlist_get_numof_str(const char** tbl);
void emlistCameraMove();
void emlistCamToPoin();
void emlistCursorToTarget();

// Enemy list editor entry (debug menu 10): init, then every frame emlist_routine[routine], the
// enemy direction arrows and (Debug_flg[0] bit 30) the debug camera.
void ToolEmList()
{
    emlist_init();
    for (;;) {
        emlistCameraMove();
        emlist_routine[EmList.wk->routine]();
        emlist_EmDir_disp();
        LightMgr.move();
        if (DbgFlagChk(pG, DBG_SCR_TEST)) {
            if (!StaFlagChk(pG, STA_BG_OFF)) {
                StaFlagOn(pG, STA_BG_OFF);
            }
            SatMgr.disp(0);
        }
        CameraMove();
        TaskSleep(1);
    }
}

// Tool start: suspends the game, default tool flags plus the enemy-list display bits, allocates
// the work (the tool exits when the Debug heap is short), cursor at the screen centre, starts in
// the main list (step 1 = list loaded).
void emlist_init()
{
    u32 i;

    TaskSuspend(0);
    TaskSleep(1);
    TutilInitDefault();
    pG->Stop_flg |= 0x200000;
    pG->Disp_flg |= 0x1000000;
    pG->Disp_flg |= 0x800000;
    DbgFlagOn(pG, DBG_TEST_MODE);
    DbgFlagOn(pG, DBG_BACK_CLIP);
    pG->Stop_flg |= 0x800000;
    DbgFlagOn(pG, DBG_DBG_CAM);
    EmListCtrl* ctl = &EmList;

    ctl->wk = (EmListWork*) Debug_alloc(sizeof(EmListWork), 1);
    if (ctl->wk == NULL) {
        for (i = 0; i < 90; i++) {
            eprintf(100, 100, 0, 0, "MEMORY ALLOCATE ERROR");
            TaskSleep(1);
        }
        emlist_exit();
    }
    EmList.wk->fileNo = 0;
    EmList.wk->cursorX = (Screen.x + Screen.width) * 0.5f;
    EmList.wk->cursorY = (Screen.y + Screen.height) * 0.5f;
    EmList.wk->idNum = 0;
    for (i = 0; i < 64; i++) {
        if (EmListIdTbl[i].name[0] == 0) {
            break;
        }
        EmList.wk->idNum++;
    }
    EmList.wk->camMode = 0;
    EmList.wk->id = 0x15;
    EmList.wk->cur.id = 0x15;
    EmList.wk->cur.id = 0;
    EmList.wk->cur.type = 0;
    EmList.wk->cur.set = 0;
    EmList.wk->cur.flags4 = 0;
    EmList.wk->cur.Character = 0;
    EmList.wk->cur.Guard_r = 10;
    EmList.wk->cur.hp = 1000;
    EmList.wk->cur.emset_no = 0;
    EmList.wk->routine = 1;
    EmList.wk->step = 0;
    EmList.wk->x8 = 0;
    EmList.wk->xC = 0;
    EmList.wk->menuNo = 0;
    EmList.wk->catchNo = -1;
    EmList.wk->step = 1;
    EmList.wk->cursorX = (Screen.x + Screen.width) * 0.5f;
    EmList.wk->cursorY = (Screen.y + Screen.height) * 0.5f;
    EmList.wk->listNo = 0;
}

// Restores the flags, frees the work and ends the task.
void emlist_exit()
{
    pG->Stop_flg &= ~0x200000;
    pG->Disp_flg &= ~0x1000000;
    pG->Disp_flg &= ~0x800000;
    DbgFlagOff(pG, DBG_TEST_MODE);
    pG->Stop_flg &= ~0x800000;
    DbgFlagOff(pG, DBG_DBG_CAM);
    TutilQuitDefault();
    TaskSignal(0);
    TaskExit();
}

// List mode: up/down step the entry, left/right by 20; L+R+Z deletes, L+Z clears, Y copies, L+R+Y
// inserts an empty entry, L+R+X inserts the copy, L+X overwrites with the copy, B menu, A target.
static void emlist_r0_main()
{
    EmListEnt* p;
    u32 i;

    if (EmList.wk->joy.rep2 & (JOY_UP | JOY_SUP)) {
        EmList.wk->listNo--;
        if (EmList.wk->listNo < 0) {
            EmList.wk->listNo = 0;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        EmList.wk->listNo++;
        if (EmList.wk->listNo > 0xFE) {
            EmList.wk->listNo = 0xFE;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_LEFT | JOY_SRIGHT)) {
        if (EmList.wk->listNo > 0x13) {
            EmList.wk->listNo -= 0x14;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_RIGHT | JOY_SLEFT)) {
        if (EmList.wk->listNo <= 0xEA) {
            EmList.wk->listNo += 0x14;
        }
    }
    p = EMLIST_ENT(EmList.wk->listNo);
    if ((EmList.wk->joy.on & (JOY_L | JOY_R)) == (JOY_L | JOY_R) && (EmList.wk->joy.trg & JOY_Z)) {
        for (i = EmList.wk->listNo; i <= 0xFD; i++) {
            EMLIST_COPY(i, i + 1);
        }
        memclr_asm(EMLIST_ENT(i), 0x20);
    } else if ((EmList.wk->joy.on & (JOY_L | JOY_R)) == JOY_L && (EmList.wk->joy.trg & JOY_Z)) {
        memclr_asm(p, 0x20);
    } else if ((EmList.wk->joy.on & (JOY_L | JOY_R)) == (JOY_L | JOY_R) && (EmList.wk->joy.trg & JOY_Y)) {
        for (i = EmList.wk->listNo + 1; i <= 0xFE; i++) {
            if (EMLIST_ENT_I(i)->id == 0) {
                break;
            }
        }
        if (i <= 0xFE) {
            for (; i > EmList.wk->listNo; i--) {
                EMLIST_COPY(i, i - 1);
            }
            memclr_asm(EMLIST_ENT(i), 0x20);
        }
    } else if ((EmList.wk->joy.on & (JOY_L | JOY_R)) == 0 && (EmList.wk->joy.trg & JOY_Y)) {
        EmList.wk->copy = *p;
    } else if ((EmList.wk->joy.on & (JOY_L | JOY_R)) == (JOY_L | JOY_R) && (EmList.wk->joy.trg & JOY_X)) {
        for (i = EmList.wk->listNo; i <= 0xFE; i++) {
            if (EMLIST_ENT_I(i)->id == 0) {
                break;
            }
        }
        if (i <= 0xFE) {
            for (; i > EmList.wk->listNo; i--) {
                EMLIST_COPY(i, i - 1);
            }
        }
        *p = EmList.wk->copy;
    } else if ((EmList.wk->joy.on & (JOY_L | JOY_R)) == JOY_L && (EmList.wk->joy.trg & JOY_X)) {
        *p = EmList.wk->copy;
    } else {
        if (EmList.wk->joy.trg & JOY_B) {
            EmList.wk->routine = 13;
            EmList.wk->step = 0;
            EmList.wk->x8 = 0;
            EmList.wk->xC = 0;
            EmList.wk->menuNo = 0;
        }
        if (EmList.wk->joy.trg & JOY_A) {
            EmList.wk->routine = 1;
            EmList.wk->step = 0;
            EmList.wk->x8 = 0;
            EmList.wk->xC = 0;
            EmList.wk->menuNo = 0;
            EmList.wk->catchNo = -1;
            EmList.wk->step = 1;
            EmList.wk->cursorX = (Screen.x + Screen.width) * 0.5f;
            EmList.wk->cursorY = (Screen.y + Screen.height) * 0.5f;
        }
    }
    emlist_main_disp();
    emlist_target_disp(0);
}

// Moves listNo to the previous (dir < 0) / next entry of the current room (unchanged when there is
// none), then aims the camera and the cursor at it.
#define EMLIST_ROOM_MATCH(p) (pG->stage_no == (p)->room >> 8 && pG->room_no == ((p)->room & 0xFF))

// Target mode. step 0: the field menu (up/down, A selects, B/Y to the cursor). step 1: the 3D cursor
// (A catches the nearest entry and drags it, A+Z deletes, X makes a new entry at the cursor, A+L/R
// rotate, C stick up/down moves it vertically, L/R step through the room's entries).
static void emlist_r0_target()
{
    Vec v;
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    int i;

    switch (EmList.wk->step) {
    default:
    case 0:
        if (EmList.wk->joy.rep2 & (JOY_UP | JOY_SUP)) {
            EmList.wk->menuNo--;
            if (EmList.wk->menuNo < 0) {
                EmList.wk->menuNo = 12;
            }
        }
        if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
            EmList.wk->menuNo++;
            if (EmList.wk->menuNo > 12) {
                EmList.wk->menuNo = 0;
            }
        }
        if (EmList.wk->joy.trg & (JOY_B | JOY_Y)) {
            EmList.wk->step = 1;
            EmList.wk->catchNo = -1;
            break;
        }
        if (EmList.wk->joy.rep2 & JOY_L) {
            int old = EmList.wk->listNo;
            if (EmList.wk->listNo > 0) {
                do {
                    EmList.wk->listNo--;
                    if (EmList.wk->listNo < 0) {
                        EmList.wk->listNo = 0;
                    }
                    p = EMLIST_ENT(EmList.wk->listNo);
                    if (EMLIST_ROOM_MATCH(p)) {
                        break;
                    }
                } while (EmList.wk->listNo > 0);
            }
            if (!EMLIST_ROOM_MATCH(p)) {
                EmList.wk->listNo = old;
            }
            p = EMLIST_ENT(EmList.wk->listNo);
            emlistCamToPoin();
            emlistCursorToTarget();
        }
        if (EmList.wk->joy.rep2 & JOY_R) {
            int old = EmList.wk->listNo;
            if (EmList.wk->listNo <= 0xFD) {
                do {
                    EmList.wk->listNo++;
                    if (EmList.wk->listNo > 0xFE) {
                        EmList.wk->listNo = 0xFE;
                    }
                    p = EMLIST_ENT(EmList.wk->listNo);
                    if (EMLIST_ROOM_MATCH(p)) {
                        break;
                    }
                } while (EmList.wk->listNo <= 0xFD);
            }
            if (!EMLIST_ROOM_MATCH(p)) {
                EmList.wk->listNo = old;
            }
            p = EMLIST_ENT(EmList.wk->listNo);
            emlistCamToPoin();
            emlistCursorToTarget();
        }
        if (EmList.wk->joy.trg & JOY_A) {
            switch (EmList.wk->menuNo) {
            case 0:
                EmList.wk->routine = 2;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                if (p->id != 0) {
                    EmList.wk->id = p->id;
                }
                break;
            case 1:
                EmList.wk->routine = 3;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                EmList.wk->subNo = 0;
                break;
            case 2:
                EmList.wk->routine = 4;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                EmList.wk->subNo = 0;
                break;
            case 3:
                EmList.wk->routine = 5;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                EmList.wk->subNo = 0;
                break;
            case 4:
                EmList.wk->routine = 6;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                EmList.wk->subNo = 0;
                break;
            case 5:
                EmList.wk->routine = 7;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                EmList.wk->subNo = 0;
                break;
            case 6:
                EmList.wk->routine = 8;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                EmList.wk->subNo = 0;
                break;
            case 7:
                EmList.wk->routine = 9;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                EmList.wk->subNo = 0;
                break;
            case 8:
                EmList.wk->routine = 10;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                EmList.wk->subNo = 0;
                break;
            case 9:
                EmList.wk->routine = 11;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                EmList.wk->subNo = 0;
                break;
            case 10:
                EmList.wk->routine = 12;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                EmList.wk->subNo = 0;
                break;
            case 11:
                EmList.wk->routine = 18;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                EmList.wk->yesNo = 0;
                break;
            case 12:
            default:
                EmList.wk->routine = 0;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
                break;
            }
        }
        emlist_target_menu_disp(0);
        break;
    case 1:
        if (EmList.wk->camMode == 0) {
            TutilMoveCursor((Vec*) &EmList.wk->cursorX, 20.0f, 1.0f);
        }
        if (EmList.wk->joy.trg & JOY_B) {
            EmList.wk->routine = 0;
            EmList.wk->step = 0;
            EmList.wk->x8 = 0;
            EmList.wk->xC = 0;
            break;
        }
        if (EmList.wk->joy.trg & JOY_Y) {
            EmList.wk->step = 0;
            break;
        }
        if (EmList.wk->joy.trg & JOY_A) {
            EmList.wk->catchNo = emlist_catch_em();
            if (EmList.wk->catchNo != -1) {
                EmList.wk->listNo = EmList.wk->catchNo;
                p = EMLIST_ENT(EmList.wk->listNo);
            }
        }
        if (EmList.wk->joy.on & JOY_A) {
            if (EmList.wk->catchNo != -1) {
                emlist_em_move_to_cursor__Fv(p);
                EmList.wk->id = p->id;
            }
            if (EmList.wk->joy.trg & JOY_Z) {
                p->id = 0;
            }
        }
        if (EmList.wk->joy.trg & JOY_X) {
            if (p->id != 0) {
                i = EmList.wk->listNo;
                while (i <= 0xFE) {
                    if (EMLIST_ENT_I(i)->id == 0) {
                        EmList.wk->listNo = i;
                        p = EMLIST_ENT(EmList.wk->listNo);
                        break;
                    }
                    i++;
                }
                if (p->id != 0) {
                    for (i = EmList.wk->listNo; i >= 0; i--) {
                        if (EMLIST_ENT_I(i)->id == 0) {
                            EmList.wk->listNo = i;
                            p = EMLIST_ENT(EmList.wk->listNo);
                            break;
                        }
                    }
                }
            }
            if (p->id == 0) {
                if (EmList.wk->cur.id == 0) {
                    EmList.wk->cur.id = 0x15;
                    EmList.wk->cur.type = 0;
                    EmList.wk->cur.set = 0;
                    EmList.wk->cur.flags4 = 0;
                    EmList.wk->cur.Character = 0;
                    EmList.wk->cur.Guard_r = 10;
                    EmList.wk->cur.hp = 1000;
                    EmList.wk->cur.rot[0] = 0;
                    EmList.wk->cur.rot[1] = 0;
                    EmList.wk->cur.rot[2] = 0;
                    EmList.wk->cur.emset_no = 0;
                }
                p->id = EmList.wk->cur.id;
                p->type = EmList.wk->cur.type;
                p->set = EmList.wk->cur.set;
                p->flags4 = EmList.wk->cur.flags4;
                p->Character = EmList.wk->cur.Character;
                p->Guard_r = EmList.wk->cur.Guard_r;
                p->hp = EmList.wk->cur.hp;
                // rot / room through byte pointers: the member forms change the pG reloads around them
                memcpy((u32*) ((u8*) p + 0x12), (u32*) ((u8*) &EmList.wk->cur + 0x12), 6);
                p->flags |= 1;
                *(u16*) ((u8*) p + 0x18) = (pG->stage_no << 8) | pG->room_no;
                p->emset_no = 0;
                PSVECSubtract(&pG->Camera.param.at, &pG->Camera.param.pos, &v);
                {
                    f32 vy = v.y;
                    v.y = 0.0f;
                    PSVECScale(&v, &v, (pPLem->pos.y - pG->Camera.param.pos.y) / vy);
                }
                PSVECAdd(&pG->Camera.param.pos, &v, &v);
                ((s16*) p->pos)[0] = (s16) (v.x * 0.1f);
                ((s16*) p->pos)[1] = (s16) (pPLem->pos.y * 0.1f);
                ((s16*) p->pos)[2] = (s16) (v.z * 0.1f);
                emlist_em_move_to_cursor__Fv(p);
            }
        }
        if (!(EmList.wk->joy.on & JOY_A)) {
            if (EmList.wk->joy.rep2 & JOY_L) {
                int old = EmList.wk->listNo;
                if (EmList.wk->listNo > 0) {
                    do {
                        EmList.wk->listNo--;
                        if (EmList.wk->listNo < 0) {
                            EmList.wk->listNo = 0;
                        }
                        p = EMLIST_ENT(EmList.wk->listNo);
                        if (EMLIST_ROOM_MATCH(p)) {
                            break;
                        }
                    } while (EmList.wk->listNo > 0);
                }
                if (!EMLIST_ROOM_MATCH(p)) {
                    EmList.wk->listNo = old;
                }
                p = EMLIST_ENT(EmList.wk->listNo);
                emlistCamToPoin();
                emlistCursorToTarget();
            }
            if (EmList.wk->joy.rep2 & JOY_R) {
                int old = EmList.wk->listNo;
                if (EmList.wk->listNo <= 0xFD) {
                    do {
                        EmList.wk->listNo++;
                        if (EmList.wk->listNo > 0xFE) {
                            EmList.wk->listNo = 0xFE;
                        }
                        p = EMLIST_ENT(EmList.wk->listNo);
                        if (EMLIST_ROOM_MATCH(p)) {
                            break;
                        }
                    } while (EmList.wk->listNo <= 0xFD);
                }
                if (!EMLIST_ROOM_MATCH(p)) {
                    EmList.wk->listNo = old;
                }
                p = EMLIST_ENT(EmList.wk->listNo);
                emlistCamToPoin();
                emlistCursorToTarget();
            }
        } else {
            // both rotations pick the step from the A button; in the L arm the compiler knows A is held
            // (else branch), which only leaves pi/64's constant-pool entry behind
            if (EmList.wk->joy.on & JOY_L) {
                f32 ang = (f32) (s16) p->rot[1] * (3.1415927f / 16384.0f);
                ang += (EmList.wk->joy.on & JOY_A) ? 3.1415927f / 32.0f : 3.1415927f / 64.0f;
                p->rot[1] = (s16) (LIMIT_ANGLE(ang) * (16384.0f / 3.1415927f));
            }
            if (EmList.wk->joy.on & JOY_R) {
                f32 ang = (f32) (s16) p->rot[1] * (3.1415927f / 16384.0f);
                ang -= (EmList.wk->joy.on & JOY_A) ? 3.1415927f / 32.0f : 3.1415927f / 64.0f;
                p->rot[1] = (s16) (LIMIT_ANGLE(ang) * (16384.0f / 3.1415927f));
            }
        }
        if (EmList.wk->joy.rep2 & JOY_SSUP) {
            p->pos[1] += 50;
        }
        if (EmList.wk->joy.rep2 & JOY_SSDOWN) {
            p->pos[1] -= 50;
        }
        TprimDraw2D(0);
        TprimDrawCursor((Vec*) &EmList.wk->cursorX, 0.0f, &list_color[(EmList.wk->joy.on >> 8) & 1]);
        break;
    }
    emlist_target_disp(1);
    if (EmList.wk->step == 1) {
        emlist_target_help_disp();
    }
    EmList.wk->cur = *p;
}

// Routine 2, ID field: the EM ID SELECT list (emlist_select_id) picks the enemy id; A applies it to
// the entry (and the working copy), B cancels; back to the target routine.
static void emlist_r0_set_id()
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);

    emlist_select_id();
    if (EmList.wk->joy.trg & JOY_A) {
        p->id = EmList.wk->id;
        EmList.wk->cur = *p;
        EmList.wk->routine = 1;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    if (EmList.wk->joy.trg & JOY_B) {
        EmList.wk->routine = 1;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    emlist_select_id_disp();
    emlist_target_disp(1);
}

// Moves the id-menu cursor: up/down by one, left/right by a column of 16.
void emlist_select_id()
{
    if (EmList.wk->joy.rep2 & (JOY_UP | 0x80000)) {
        EmList.wk->id--;
        if (EmList.wk->id <= 1) {
            EmList.wk->id = 2;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_DOWN | 0x40000)) {
        EmList.wk->id++;
        if (EmList.wk->id >= EmList.wk->idNum) {
            EmList.wk->id = EmList.wk->idNum - 1;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_LEFT | 0x10000)) {
        if (EmList.wk->id > 0x11) {
            EmList.wk->id -= 0x10;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_RIGHT | 0x20000)) {
        if (EmList.wk->id < EmList.wk->idNum - 0x10) {
            EmList.wk->id += 0x10;
        }
    }
}

// Left/right pick the stage or room byte (subNo), up/down step it.
static void emlist_r0_set_room()
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    int step;
    int room;

    if (EmList.wk->joy.trg & (JOY_LEFT | JOY_RIGHT | JOY_SRIGHT | JOY_SLEFT)) {
        EmList.wk->subNo ^= 1;
    }
    if (EmList.wk->subNo != 0) {
        step = 1;
        if (EmList.wk->joy.on & JOY_R) {
            step = 0x10;
        }
        if (EmList.wk->joy.rep2 & (JOY_UP | JOY_SUP)) {
            room = p->room;
            p->room = ((step + room) & 0xFF) | (room & 0xF00);
        }
        if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
            room = p->room;
            p->room = ((room - step) & 0xFF) | (room & 0xF00);
        }
    } else {
        if (EmList.wk->joy.rep2 & (JOY_UP | JOY_SUP)) {
            room = p->room;
            p->room = ((room + 0x100) & 0xF00) | (room & 0xFF);
        }
        if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
            room = p->room;
            p->room = ((room - 0x100) & 0xF00) | (room & 0xFF);
        }
    }
    if (EmList.wk->joy.trg & JOY_B) {
        EmList.wk->routine = 1;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    emlist_target_menu_disp(1);
    emlist_target_disp(1);
}

// Left/right pick the axis (subNo), up/down step it by 1 / 10 (R) / 100 (L).
static void emlist_r0_set_pos()
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    int step;
    int v;

    if (EmList.wk->joy.trg & (JOY_LEFT | JOY_SRIGHT)) {
        EmList.wk->subNo--;
        if (EmList.wk->subNo < 0) {
            EmList.wk->subNo = 2;
        }
    }
    if (EmList.wk->joy.trg & (JOY_RIGHT | JOY_SLEFT)) {
        EmList.wk->subNo++;
        if (EmList.wk->subNo > 2) {
            EmList.wk->subNo = 0;
        }
    }
    step = 0;
    if (EmList.wk->joy.rep2 & (JOY_UP | JOY_SUP)) {
        step = 1;
        if (EmList.wk->joy.on & JOY_R) {
            step = 10;
        }
        if (EmList.wk->joy.on & JOY_L) {
            step = 100;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        step = -1;
        if (EmList.wk->joy.on & JOY_R) {
            step = -10;
        }
        if (EmList.wk->joy.on & JOY_L) {
            step = -100;
        }
    }
    switch (EmList.wk->subNo) {
    case 0:
        v = p->pos[0];
        p->pos[0] = step + v;
        break;
    case 1:
        v = p->pos[1];
        p->pos[1] = step + v;
        break;
    case 2:
        v = p->pos[2];
        p->pos[2] = step + v;
        break;
    }
    if (EmList.wk->joy.trg & JOY_B) {
        EmList.wk->routine = 1;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    emlist_target_menu_disp(1);
    emlist_target_disp(1);
}

// Same for the rotation.
static void emlist_r0_set_ang()
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    int step;
    int v;

    if (EmList.wk->joy.trg & (JOY_LEFT | JOY_SRIGHT)) {
        EmList.wk->subNo--;
        if (EmList.wk->subNo < 0) {
            EmList.wk->subNo = 2;
        }
    }
    if (EmList.wk->joy.trg & (JOY_RIGHT | JOY_SLEFT)) {
        EmList.wk->subNo++;
        if (EmList.wk->subNo > 2) {
            EmList.wk->subNo = 0;
        }
    }
    step = 0;
    if (EmList.wk->joy.rep2 & (JOY_UP | JOY_SUP)) {
        step = 1;
        if (EmList.wk->joy.on & JOY_R) {
            step = 10;
        }
        if (EmList.wk->joy.on & JOY_L) {
            step = 100;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        step = -1;
        if (EmList.wk->joy.on & JOY_R) {
            step = -10;
        }
        if (EmList.wk->joy.on & JOY_L) {
            step = -100;
        }
    }
    switch (EmList.wk->subNo) {
    case 0:
        v = p->rot[0];
        p->rot[0] = step + v;
        break;
    case 1:
        v = p->rot[1];
        p->rot[1] = step + v;
        break;
    case 2:
        v = p->rot[2];
        p->rot[2] = step + v;
        break;
    }
    if (EmList.wk->joy.trg & JOY_B) {
        EmList.wk->routine = 1;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    emlist_target_menu_disp(1);
    emlist_target_disp(1);
}

// Bit cursor in subNo (0..7, bit 7 - subNo of the entry's flags), left/right toggle it.
static void emlist_r0_set_be_flag()
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    u8 bit;

    if (EmList.wk->joy.rep2 & (JOY_RIGHT | JOY_SLEFT)) {
        EmList.wk->subNo++;
        if (EmList.wk->subNo > 7) {
            EmList.wk->subNo = 0;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_LEFT | JOY_SRIGHT)) {
        EmList.wk->subNo--;
        if (EmList.wk->subNo < 0) {
            EmList.wk->subNo = 7;
        }
    }
    bit = 0x80 >> EmList.wk->subNo;
    if (EmList.wk->joy.rep2 & (JOY_UP | JOY_DOWN | JOY_SUP | JOY_SDOWN)) {
        p->flags ^= bit;
    }
    if (EmList.wk->joy.trg & (JOY_A | JOY_B)) {
        EmList.wk->routine = 1;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    emlist_target_menu_disp(1);
    emlist_target_disp(1);
}

// Routine 7, TYPE field: up/down +-1 (left/right +-0x10) on the entry's type byte, named by the
// enemy's type table; A/B back.
static void emlist_r0_set_type()
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);

    if (EmList.wk->joy.rep2 & (JOY_UP | JOY_SUP)) {
        if (EmList.wk->joy.on & JOY_R) {
            p->type += 0x10;
        } else {
            p->type += 1;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        if (EmList.wk->joy.on & JOY_R) {
            p->type -= 0x10;
        } else {
            p->type -= 1;
        }
    }
    if (EmList.wk->joy.trg & (JOY_A | JOY_B)) {
        EmList.wk->cur = *p;
        EmList.wk->routine = 1;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    emlist_target_menu_disp(1);
    emlist_target_disp(1);
}

// Routine 8, SET field: up/down step the entry's set byte through the enemy's set names; A/B back.
static void emlist_r0_set_set()
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);

    if (EmList.wk->joy.rep2 & (JOY_UP | JOY_SUP)) {
        if (EmList.wk->joy.on & JOY_R) {
            p->set += 0x10;
        } else {
            p->set += 1;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        if (EmList.wk->joy.on & JOY_R) {
            p->set -= 0x10;
        } else {
            p->set -= 1;
        }
    }
    if (EmList.wk->joy.trg & (JOY_A | JOY_B)) {
        EmList.wk->cur = *p;
        EmList.wk->routine = 1;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    emlist_target_menu_disp(1);
    emlist_target_disp(1);
}

// Bit cursor in subNo (0..31, bit 31 - subNo of flags4), up/down toggle it.
static void emlist_r0_set_em_flag()
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    u32 bit;

    if (EmList.wk->joy.rep2 & (JOY_RIGHT | JOY_SLEFT)) {
        EmList.wk->subNo++;
        if (EmList.wk->subNo > 31) {
            EmList.wk->subNo = 0;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_LEFT | JOY_SRIGHT)) {
        EmList.wk->subNo--;
        if (EmList.wk->subNo < 0) {
            EmList.wk->subNo = 31;
        }
    }
    bit = 0x80000000 >> EmList.wk->subNo;
    if (EmList.wk->joy.rep2 & (JOY_UP | JOY_DOWN | JOY_SUP | JOY_SDOWN)) {
        p->flags4 ^= bit;
    }
    if (EmList.wk->joy.trg & (JOY_A | JOY_B)) {
        EmList.wk->cur = *p;
        EmList.wk->routine = 1;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    emlist_target_menu_disp(1);
    emlist_target_disp(1);
}

// Routine 10, CHARA field: up/down step the entry's character byte (the enemy's chr names); A/B
// back.
static void emlist_r0_set_char()
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);

    if (EmList.wk->joy.rep2 & (JOY_UP | JOY_SUP)) {
        if (EmList.wk->joy.on & JOY_R) {
            p->Character += 0x10;
        } else {
            p->Character += 1;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        if (EmList.wk->joy.on & JOY_R) {
            p->Character -= 0x10;
        } else {
            p->Character -= 1;
        }
    }
    if (EmList.wk->joy.trg & (JOY_A | JOY_B)) {
        EmList.wk->cur = *p;
        EmList.wk->routine = 1;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    emlist_target_menu_disp(1);
    emlist_target_disp(1);
}

// Routine 11, HP field: up/down +-1 / +-100 (with X +-10 / +-1000) on the entry's hp; A/B back.
static void emlist_r0_set_hp()
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);

    if (EmList.wk->joy.rep2 & (JOY_UP | JOY_RIGHT | JOY_SUP | JOY_SLEFT)) {
        if (EmList.wk->joy.on & JOY_R) {
            if (EmList.wk->joy.on & JOY_L) {
                p->hp += 1000;
            } else {
                p->hp += 10;
            }
        } else if (EmList.wk->joy.on & JOY_L) {
            p->hp += 100;
        } else {
            p->hp += 1;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_LEFT | JOY_SDOWN | JOY_SRIGHT)) {
        if (EmList.wk->joy.on & JOY_R) {
            if (EmList.wk->joy.on & JOY_L) {
                p->hp -= 1000;
            } else {
                p->hp -= 10;
            }
        } else if (EmList.wk->joy.on & JOY_L) {
            p->hp -= 100;
        } else {
            p->hp -= 1;
        }
    }
    if (EmList.wk->joy.trg & (JOY_A | JOY_B)) {
        EmList.wk->cur = *p;
        EmList.wk->routine = 1;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    emlist_target_menu_disp(1);
    emlist_target_disp(1);
}

// Routine 12, Guard R field: up/right / down/left change the guard radius (metres); A/B back.
static void emlist_r0_set_guard_r()
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);

    if (EmList.wk->joy.rep2 & (JOY_UP | JOY_RIGHT | JOY_SUP | JOY_SLEFT)) {
        if (EmList.wk->joy.on & JOY_R) {
            if (EmList.wk->joy.on & JOY_L) {
                p->Guard_r += 20;
            } else {
                p->Guard_r += 5;
            }
        } else if (EmList.wk->joy.on & JOY_L) {
            p->Guard_r += 10;
        } else {
            p->Guard_r += 1;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_LEFT | JOY_SDOWN | JOY_SRIGHT)) {
        if (EmList.wk->joy.on & JOY_R) {
            if (EmList.wk->joy.on & JOY_L) {
                p->Guard_r -= 20;
            } else {
                p->Guard_r -= 5;
            }
        } else if (EmList.wk->joy.on & JOY_L) {
            p->Guard_r -= 10;
        } else {
            p->Guard_r -= 1;
        }
    }
    if (p->Guard_r <= 0) {
        p->Guard_r = 0;
    }
    if (EmList.wk->joy.trg & (JOY_A | JOY_B)) {
        EmList.wk->cur = *p;
        EmList.wk->routine = 1;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    emlist_target_menu_disp(1);
    emlist_target_disp(1);
}

// Routine 13, -- MENU --: LIST (back), SORT LIST (17), CLEAR LIST (16), SET AND EXIT (18), LOAD
// (15), SAVE (14, only after a load), EXIT; up/down, A picks, B back to the list.
static void emlist_r0_menu()
{
    if (EmList.wk->joy.rep2 & (JOY_UP | JOY_SUP)) {
        EmList.wk->menuNo--;
        if (EmList.wk->menuNo < 0) {
            EmList.wk->menuNo = 6;
        }
    }
    if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        EmList.wk->menuNo++;
        if (EmList.wk->menuNo > 6) {
            EmList.wk->menuNo = 0;
        }
    }
    if (EmList.wk->joy.trg & JOY_B) {
        EmList.wk->menuNo = 6;
    }
    if (EmList.wk->joy.trg & JOY_A) {
        switch (EmList.wk->menuNo) {
        default:
        case 0:
            EmList.wk->routine = 0;
            EmList.wk->step = 0;
            EmList.wk->x8 = 0;
            EmList.wk->xC = 0;
            break;
        case 2:
            EmList.wk->routine = 16;
            EmList.wk->step = 0;
            EmList.wk->x8 = 0;
            EmList.wk->xC = 0;
            EmList.wk->yesNo = 0;
            break;
        case 1:
            EmList.wk->routine = 17;
            EmList.wk->step = 0;
            EmList.wk->x8 = 0;
            EmList.wk->xC = 0;
            EmList.wk->yesNo = 0;
            break;
        case 3:
            EmList.wk->routine = 18;
            EmList.wk->step = 0;
            EmList.wk->x8 = 0;
            EmList.wk->xC = 0;
            EmList.wk->yesNo = 0;
            break;
        case 4:
            EmList.wk->routine = 15;
            EmList.wk->step = 0;
            EmList.wk->x8 = 0;
            EmList.wk->xC = 0;
            EmList.wk->fileSel = 0;
            break;
        case 5:
            if (EmList.wk->fileNo != 0) {
                EmList.wk->routine = 14;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
            }
            break;
        case 6:
            emlist_exit();
            break;
        }
    }
    emlist_menu_disp();
}

// File menu: step 0 picks the file (fileSel, 0 = cancel, 1..30), step 1 asks yes/no (yesNo).
static void emlist_r0_save()
{
    int step = EmList.wk->step;

    switch (step) {
    case 0:
        if (EmList.wk->joy.rep2 & (JOY_UP | JOY_SUP)) {
            EmList.wk->fileSel--;
            if (EmList.wk->fileSel < 0) {
                EmList.wk->fileSel = 30;
            }
        }
        if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
            EmList.wk->fileSel++;
            if (EmList.wk->fileSel > 30) {
                EmList.wk->fileSel = 0;
            }
        }
        if (EmList.wk->joy.rep2 & (JOY_LEFT | JOY_SRIGHT)) {
            if (EmList.wk->fileSel > 10) {
                EmList.wk->fileSel -= 10;
            }
        }
        if (EmList.wk->joy.rep2 & (JOY_RIGHT | JOY_SLEFT)) {
            if (EmList.wk->fileSel >= 1 && EmList.wk->fileSel <= 20) {
                EmList.wk->fileSel += 10;
            }
        }
        if (EmList.wk->joy.trg & JOY_B) {
            EmList.wk->routine = 13;
            EmList.wk->step = 0;
            EmList.wk->x8 = 0;
            EmList.wk->xC = 0;
        } else if (EmList.wk->joy.trg & JOY_A) {
            if (EmList.wk->fileSel == 0) {
                EmList.wk->routine = 13;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
            } else {
                EmList.wk->step = 1;
                EmList.wk->yesNo = 0;
            }
        }
        break;
    case 1:
        if (EmList.wk->joy.rep2 & (JOY_LEFT | JOY_RIGHT | JOY_SRIGHT | JOY_SLEFT)) {
            EmList.wk->yesNo ^= 1;
        }
        if (EmList.wk->joy.trg & JOY_B) {
            EmList.wk->step = 0;
            break;
        }
        if (EmList.wk->joy.trg & JOY_A) {
            if (EmList.wk->yesNo == 0) {
                EmList.wk->step = 0;
            } else {
                EmList.wk->step = 1;
                emlist_file_save(EmList.wk->fileSel - 1);
                EmList.wk->routine = 13;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
            }
        }
        emlist_yes_no_menu_disp(0x17C);
        break;
    }
    emlist_menu_disp();
    eprintf(0x28, 0xB4, 0, 0, "-- SAVE --------");
    emlist_file_menu_disp();
}

// File menu: step 0 picks the file (fileSel, 0 = cancel, 1..30), step 1 asks yes/no (yesNo).
static void emlist_r0_load()
{
    int step = EmList.wk->step;

    switch (step) {
    case 0:
        if (EmList.wk->joy.rep2 & (JOY_UP | JOY_SUP)) {
            EmList.wk->fileSel--;
            if (EmList.wk->fileSel < 0) {
                EmList.wk->fileSel = 30;
            }
        }
        if (EmList.wk->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
            EmList.wk->fileSel++;
            if (EmList.wk->fileSel > 30) {
                EmList.wk->fileSel = 0;
            }
        }
        if (EmList.wk->joy.rep2 & (JOY_LEFT | JOY_SRIGHT)) {
            if (EmList.wk->fileSel > 10) {
                EmList.wk->fileSel -= 10;
            }
        }
        if (EmList.wk->joy.rep2 & (JOY_RIGHT | JOY_SLEFT)) {
            if (EmList.wk->fileSel >= 1 && EmList.wk->fileSel <= 20) {
                EmList.wk->fileSel += 10;
            }
        }
        if (EmList.wk->joy.trg & JOY_B) {
            EmList.wk->routine = 13;
            EmList.wk->step = 0;
            EmList.wk->x8 = 0;
            EmList.wk->xC = 0;
        } else if (EmList.wk->joy.trg & JOY_A) {
            if (EmList.wk->fileSel == 0) {
                EmList.wk->routine = 13;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
            } else {
                EmList.wk->step = 1;
                EmList.wk->yesNo = 0;
            }
        }
        break;
    case 1:
        if (EmList.wk->joy.rep2 & (JOY_LEFT | JOY_RIGHT | JOY_SRIGHT | JOY_SLEFT)) {
            EmList.wk->yesNo ^= 1;
        }
        if (EmList.wk->joy.trg & JOY_B) {
            EmList.wk->step = 0;
            break;
        }
        if (EmList.wk->joy.trg & JOY_A) {
            if (EmList.wk->yesNo == 0) {
                EmList.wk->step = 0;
            } else {
                EmList.wk->step = 1;
                emlist_file_load(EmList.wk->fileSel - 1);
                EmList.wk->routine = 13;
                EmList.wk->step = 0;
                EmList.wk->x8 = 0;
                EmList.wk->xC = 0;
            }
        }
        emlist_yes_no_menu_disp(0x17C);
        break;
    }
    emlist_menu_disp();
    eprintf(0x28, 0xB4, 0, 0, "-- LOAD --------");
    emlist_file_menu_disp();
}

// "Initialize List ?" yes/no (yesNo), then back to the menu.
static void emlist_r0_clear()
{
    if (EmList.wk->joy.rep2 & (JOY_LEFT | JOY_RIGHT | JOY_SRIGHT | JOY_SLEFT)) {
        EmList.wk->yesNo ^= 1;
    }
    if (EmList.wk->joy.trg & JOY_B) {
        EmList.wk->routine = 13;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
        return;
    }
    if (EmList.wk->joy.trg & JOY_A) {
        if (EmList.wk->yesNo != 0) {
            memclr_asm(pG->Em_list, 0x1FE0);
        }
        EmList.wk->routine = 13;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    eprintf(0x28, 0xB4, 0, 0, "Initialize List ?");
    emlist_yes_no_menu_disp(0xC8);
}

// "Sort List ?" yes/no (yesNo): bubble-sorts the list by room, empty entries last.
static void emlist_r0_sort()
{
    u8 tmp[0x20];
    u32 i;
    u32 j;

    if (EmList.wk->joy.rep2 & (JOY_LEFT | JOY_RIGHT | JOY_SRIGHT | JOY_SLEFT)) {
        EmList.wk->yesNo ^= 1;
    }
    if (EmList.wk->joy.trg & JOY_B) {
        EmList.wk->routine = 13;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
        return;
    }
    if (EmList.wk->joy.trg & JOY_A) {
        if (EmList.wk->yesNo != 0) {
            for (i = 0; i <= 0xFD; i++) {
                for (j = i + 1; j <= 0xFE; j++) {
                    if (EMLIST_ENT_I(i)->room > EMLIST_ENT_I(j)->room && EMLIST_ENT_I(j)->id != 0) {
                        memcpy(tmp, &pG->Em_list[i], 0x20);
                        EMLIST_COPY(i, j);
                        memcpy(&pG->Em_list[j], tmp, 0x20);
                    } else if (EMLIST_ENT_I(i)->id == 0 && EMLIST_ENT_I(j)->id != 0) {
                        memcpy(tmp, &pG->Em_list[i], 0x20);
                        EMLIST_COPY(i, j);
                        memcpy(&pG->Em_list[j], tmp, 0x20);
                    }
                }
            }
        }
        EmList.wk->routine = 13;
        EmList.wk->step = 0;
        EmList.wk->x8 = 0;
        EmList.wk->xC = 0;
    }
    eprintf(0x28, 0xB4, 0, 0, "Sort List ?");
    emlist_yes_no_menu_disp(0xC8);
}

// Clears the death bits of the current list and re-creates its enemies.
static void emlist_r0_set_exit()
{
    u32 i;

    for (i = 0; i < 255; i++) {
        int no = pG->em_list_no;
        if (no >= 0) {
            u32* tbl = EM_FLG_ROW(no);  // pG->Em_flg[no], tool style
            (tbl[i >> 5] &= ~(0x80000000 >> (i & 0x1F)));
        }
    }
    EmSetFromList();
    emlist_exit();
}

// List page (40 entries in two columns) with the key help and the copy buffer.
void emlist_main_disp()
{
    EmListIdInfo info;
    EmListEnt* p;
    int page = EmList.wk->listNo / 40;
    int base;
    int i;
    int j;
    u32 no;
    int x;
    int y;
    int col;

    if (EmList.wk->fileNo == 0) {
        eprintf(0x28, 0x1E, 0, 0, "Select File No.[ -- ]");
    } else {
        eprintf(0x28, 0x1E, 0, 0, "Select File No.[ %02d ]", EmList.wk->fileNo - 1);
    }
    eprintf2(7, 0x10, 0x198, 0x1E, 0, 0, "START CAMERA");
    eprintf2(7, 0x10, 0x198, 0x2E, 0, 0, "A     Select");
    eprintf2(7, 0x10, 0x198, 0x3E, 0, 0, "Y     Copy");
    eprintf2(7, 0x10, 0x198, 0x4E, 0, 0, "L+X   O.Write");
    eprintf2(7, 0x10, 0x198, 0x5E, 0, 0, "L+Z   Clear");
    eprintf2(7, 0x10, 0x198, 0x6E, 0, 0, "L+R+Z Delete");
    eprintf2(7, 0x10, 0x198, 0x7E, 0, 0, "L+R+Y Insert");
    eprintf2(7, 0x10, 0x198, 0x8E, 0, 0, "L+R+X Paste");
    eprintf2(7, 0x10, 0x198, 0x9E, 0, 0, "B     Menu");
    eprintf2(7, 0x10, 0x198, 0xDC, 0, 0, "copy data");
    if (EmList.wk->copy.id != 0) {
        p = &EmList.wk->copy;
        eprintf2(7, 0x10, 0x198, 0xEC, 4, 0, "R%03x", p->room);
        base = page * 40;
        info = EmListIdTbl[p->id];
        eprintf2(7, 0x10, 0x198, 0xFC, 4, 0, "%s", info.name);
    } else {
        eprintf2(7, 0x10, 0x198, 0xEC, 0, 0, "----");
        base = page * 40;
        eprintf2(7, 0x10, 0x198, 0xFC, 0, 0, "----------------");
    }
    for (j = 0; j < 2; j++) {
        for (i = 0; i < 20; i++) {
            no = i + (base + j * 20);
            x = j * 0xBC + 0x16;
            y = i * 0x10 + 0x32;
            if (no > 0xFE) {
                break;
            }
            p = EMLIST_ENT(no);
            col = 7;
            if (pG->stage_no == p->room >> 8 && pG->room_no == (p->room & 0xFF) && p->id != 0) {
                col = 0;
            }
            if (no == EmList.wk->listNo) {
                col = 4;
            }
            if (p->id == 0) {
                eprintf(x, y, col, 0, "%02d:----------------", no);
            } else {
                if (pG->stage_no == p->room >> 8 && pG->room_no == (p->room & 0xFF)) {
                    if (p->flags & 1) {
                        eprintf2(7, 0xE, x, y, 2, 0, "%03d:", no);
                    } else {
                        eprintf2(7, 0xE, x, y, 0, 0, "%03d:", no);
                    }
                } else {
                    eprintf2(7, 0xE, x, y, col, 0, "%03d:", no);
                }
                if (no == EmList.wk->listNo) {
                    info = EmListIdTbl[p->id];
                    eprintf2(8, 0x10, x + 0x1C, y, col, 0, "R%03x EM%02x %s", p->room, p->id, info.name);
                } else {
                    info = EmListIdTbl[p->id];
                    eprintf2(7, 0xE, x + 0x1C, y, col, 0, "r%03x EM%02x %s", p->room, p->id, info.name);
                }
            }
        }
    }
}

// Help column of the list screen: START camera, Y set menu, A catch, A+Z delete, A+L/R rotate, X
// new entry, C stick up/down height.
void emlist_target_help_disp()
{
    eprintf(0x180, 0x3C, 0, 0, "START  CAMERA");
    eprintf(0x180, 0x4C, 0, 0, "Y      Set menu");
    eprintf(0x180, 0x5C, 0, 0, "A      Catch");
    eprintf(0x180, 0x6C, 0, 0, "A+Z    Delete");
    eprintf(0x180, 0x7C, 0, 0, "A+L    Rot L");
    eprintf(0x180, 0x8C, 0, 0, "A+R    Rot R");
    eprintf(0x180, 0x9C, 0, 0, "X      New set ");
    eprintf(0x180, 0xAC, 0, 0, "C up   Pos up");
    eprintf(0x180, 0xBC, 0, 0, "C down Pos down");
    eprintf(0x180, 0xCC, 0, 0, "B      To List");
    eprintf(0x180, 0xDC, 0, 0, "L      Target--");
    eprintf(0x180, 0xEC, 0, 0, "R      Target++");
}

// Current entry: id line, position/angle line and the field dump (only the id line in list mode
// for an empty entry).
void emlist_target_disp(int flag)
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    Vec ang;
    EmListIdInfo info;

    if (p->id == 0) {
        if (flag == 0) {
            eprintf(0x28, 0x174, 0, 0, "List No.[ %02d / %02d ]", EmList.wk->listNo, 0xFE);
            return;
        }
        eprintf(0x28, 0x174, 0, 0, "List No.[ %02d / %02d ] -->> Id[ ---------------- ]", EmList.wk->listNo, 0xFE);
    } else {
        info = EmListIdTbl[p->id];
        eprintf(0x28, 0x174, 0, 0, "List No.[ %02d / %02d ] -->> Id[ EM%02x:%s ]", EmList.wk->listNo, 0xFE, p->id,
                info.name);
    }
    ang.x = (f32) (s16) p->rot[0] * (3.1415927f / 16384.0f);
    ang.y = (f32) (s16) p->rot[1] * (3.1415927f / 16384.0f);
    ang.z = (f32) (s16) p->rot[2] * (3.1415927f / 16384.0f);
    eprintf(0x28, 0x184, 0, 0, "Pos[ %d, %d, %d]   Ang[ %f, %f, %f ]", (s16) p->pos[0] * 10, (s16) p->pos[1] * 10,
            (s16) p->pos[2] * 10, ang.x, ang.y, ang.z);
    eprintf(0x28, 0x194, 4, 0, "EmSet: Be_flag: Type: Set: Flag      HP:   Chara");
    eprintf(0x28, 0x1A4, 0, 0, "%04x:  %02x:      %02x:   %02x:  %08x: %04d: %02x", p->emset_no, p->flags, p->type, p->set,
            p->flags4, p->hp, p->Character);
}

// Target menu: the 13 field lines, each drawn by its disp function (the cursor line highlighted).
void emlist_target_menu_disp(int flag)
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    EmListIdInfo info;
    u32 i;
    int y;
    int sel;

    eprintf(0x28, 0x3C, 0, 0, "-- TARGET MENU --------");
    if (pG->stage_no == p->room >> 8 && pG->room_no == (p->room & 0xFF)) {
        eprintf(0x28, 0x50, 4, 0, "[ No. = %03d ]", EmList.wk->listNo);
    } else {
        eprintf(0x28, 0x50, 7, 0, "[ No. = %03d ]", EmList.wk->listNo);
    }
    for (i = 0; i <= 12; i++) {
        y = 0x60 + i * 0x10;
        eprintf(0x28, y, (EmList.wk->menuNo == i) ? 4 : 0, 0, "%s", target_menu[i]);
        if (EmList.wk->menuNo == i) {
            eprintf(0x20, y, 0, 0, ">");
        }
        sel = 0;
        if (flag) {
            sel = (i == EmList.wk->menuNo);
        }
        switch (i) {
        case 0:
            if (p->id == 0) {
                eprintf(0x68, y, 0, 0, "------------");
            } else {
                info = EmListIdTbl[p->id];
                eprintf(0x68, y, 0, 0, "%s", info.name);
            }
            break;
        case 1:
            emlist_set_room_disp(0x68, y, sel);
            break;
        case 2:
            emlist_set_pos_disp(0x68, y, sel);
            break;
        case 3:
            emlist_set_ang_disp(0x68, y, sel);
            break;
        case 4:
            emlist_set_be_flag_disp(0x68, y, sel);
            break;
        case 5:
            emlist_set_type_disp(0x68, y, sel);
            break;
        case 6:
            emlist_set_set_disp(0x68, y, sel);
            break;
        case 7:
            emlist_set_em_flag_disp(0x68, y, sel);
            break;
        case 8:
            emlist_set_char_disp(0x68, y, sel);
            break;
        case 9:
            emlist_set_hp_disp(0x68, y, sel);
            break;
        case 10:
            emlist_set_guard_r_disp(0x68, y, sel);
            break;
        }
    }
}

// Draws the main menu rows with the cursor ("Not load file! Don't save" on SAVE before a load).
void emlist_menu_disp()
{
    int i;
    int y;
    int col;

    eprintf(0x28, 0x3C, 0, 0, "-- MENU --------");
    for (i = 0; i <= 6; i++) {
        y = 0x50 + i * 0x10;
        col = (EmList.wk->menuNo == i) ? 4 : 0;
        if (EmList.wk->fileNo == 0 && i == 5) {
            col = 7;
            eprintf(100, y, 2, 0, "Not load file!   Don't save");
        }
        eprintf(0x28, y, col, 0, "%s", main_menu[i]);
        if (EmList.wk->menuNo == i) {
            eprintf(0x20, y, 0, 0, ">");
        }
    }
}

// Three columns of file slots: cancel, emlist00..09, the stage lists (emlen), the omake lists.
// OPEN: the original copies the `i - 11` giv into r8 once before the compare tree and shares one
// eprintf tail between the omake and the stage cases; ours folds the argument per case.
void emlist_file_menu_disp()
{
    int i;
    int x;
    int y;
    int col;

    x = 0x28;
    y = 0xC8;
    for (i = 0; i <= 30; y += 0x10, i++) {
        if (i == 11 || i == 21) {
            x += 0xA0;
            y = 0xD8;
        }
        col = (EmList.wk->fileSel == i) ? 4 : 0;
        if (i == 0) {
            eprintf(x, y, col, 0, "CANCEL");
        } else {
            if (i > 10) {
                if (i > 20) {
                    switch (i) {
                    default:
                        eprintf(x, y, col, 0, "omake%02d.esl", i - 21);
                        break;
                    case 21:
                        eprintf(x, y, col, 0, "omake ADA");
                        break;
                    case 22:
                        eprintf(x, y, col, 0, "omake ETC");
                        break;
                    case 23:
                        eprintf(x, y, col, 0, "omake ETC2");
                        break;
                    }
                } else {
                    switch (i - 11) {
                    default:
                        eprintf(x, y, col, 0, "emlen%02d.esl", i - 11);
                        break;
                    case 0:
                        eprintf(x, y, col, 0, "Stage 1 Day");
                        break;
                    case 1:
                        eprintf(x, y, col, 0, "Stage 1 Night");
                        break;
                    case 2:
                        eprintf(x, y, col, 0, "Stage 2 -1st-");
                        break;
                    case 3:
                        eprintf(x, y, col, 0, "Stage 2 -2nd-");
                        break;
                    case 4:
                        eprintf(x, y, col, 0, "Stage 2 -3rd-");
                        break;
                    case 5:
                        eprintf(x, y, col, 0, "Stage 2 -4th-");
                        break;
                    case 6:
                        eprintf(x, y, col, 0, "Stage 3 -1st-");
                        break;
                    case 7:
                        eprintf(x, y, col, 0, "Stage 3 -2nd-");
                        break;
                    }
                }
            } else {
                eprintf(x, y, col, 0, "emlist%02d.esl", i - 1);
            }
            if (EmList.wk->fileNo == i) {
                eprintf(x + 0x60 + pG->Frame_cnt % 10, y, 4, 0, "<- old");
            }
        }
        if (EmList.wk->fileSel == i) {
            eprintf(x - 8, y, 0, 0, ">");
        }
    }
}

// "Ok? yes / no" prompt row with the cursor.
void emlist_yes_no_menu_disp(int y)
{
    eprintf(0x28, y, 4, 0, "Ok?");
    eprintf(0x82, y, 0, 0, "/");
    if (EmList.wk->yesNo) {
        eprintf(0x5A, y, 4, 0, "YES");
        eprintf(0x9A, y, 0, 0, "no");
        eprintf(0x52, y, 0, 0, ">");
    } else {
        eprintf(0x5A, y, 0, 0, "yes");
        eprintf(0x9A, y, 4, 0, "NO");
        eprintf(0x92, y, 0, 0, ">");
    }
}

// -- EM ID SELECT -- list: the enemy ids with their names, the cursor row highlighted.
void emlist_select_id_disp()
{
    int i;
    int x;
    int y;
    int col;
    EmListIdInfo* p;

    eprintf(0x28, 0x1E, 0, 0, "-- EM ID SELECT --------");
    x = 0x1E;
    y = 0x3C;
    for (i = 0; i < EmList.wk->idNum; i++) {
        p = &EmListIdTbl[i];
        if (i != 0 && (i & 0xF) == 0) {
            x += 0x78;
            y = 0x3C;
        }
        col = (i == EmList.wk->id) ? 4 : 7;
        if (p == NULL) {
            break;
        }
        if (i <= 1) {
            eprintf(x, y, 7, 0, "-----------");
        } else {
            eprintf2(10, 18, x, y, col, 0, "%s", p->name);
        }
        y += 0x12;
    }
}

// ROOM field text: the entry's room (stage/room) with the current room marked.
void emlist_set_room_disp(int x, int y, int flag)
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    int col1;
    int col2;

    if (flag) {
        if (EmList.wk->subNo) {
            col1 = 0;
            col2 = 4;
        } else {
            col1 = 4;
            col2 = 0;
        }
    } else if (pG->stage_no == p->room >> 8 && pG->room_no == (p->room & 0xFF)) {
        col1 = 0;
        col2 = 0;
    } else {
        col1 = 7;
        col2 = 7;
    }
    if (flag) {
        if (EmList.wk->subNo) {
            eprintf(x + 100, y, 0, 0, ">");
        } else {
            eprintf(x, y, 0, 0, ">");
        }
    }
    eprintf(x + 8, y, col1, 0, "Stage = %1x", p->room >> 8);
    eprintf(x + 0x6C, y, col2, 0, "Room = %02x", p->room & 0xFF);
}

// Position in mm (the list stores cm); the edited axis is highlighted.
void emlist_set_pos_disp(int x, int y, int flag)
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    u8 col[3];

    col[0] = 0;
    col[1] = 0;
    col[2] = 0;
    if (flag) {
        col[EmList.wk->subNo] = 4;
    }
    x += 8;
    eprintf(x, y, col[0], 0, "[ %5d,", (s16) p->pos[0] * 10);
    eprintf(x + 0x48, y, col[1], 0, "%5d,", (s16) p->pos[1] * 10);
    eprintf(x + 0x80, y, col[2], 0, "%5d ]", (s16) p->pos[2] * 10);
}

// Rotation (raw 16-bit angles; the radian conversion is computed but not printed).
void emlist_set_ang_disp(int x, int y, int flag)
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    u8 col[3];
    Vec ang;

    col[0] = 0;
    col[1] = 0;
    col[2] = 0;
    if (flag) {
        col[EmList.wk->subNo] = 4;
    }
    x += 8;
    ang.x = (f32) (s16) p->rot[0] * (3.1415927f / 16384.0f);
    ang.y = (f32) (s16) p->rot[1] * (3.1415927f / 16384.0f);
    ang.z = (f32) (s16) p->rot[2] * (3.1415927f / 16384.0f);
    eprintf(x, y, col[0], 0, "[ %5d,", (s16) p->rot[0]);
    eprintf(x + 0x48, y, col[1], 0, "%5d,", (s16) p->rot[1]);
    eprintf(x + 0x80, y, col[2], 0, "%5d ]", (s16) p->rot[2]);
}

// The 8 be_flag bits as 0/1 digits (the bit under the cursor highlighted), then the names of the
// set bits and the name of the bit under the cursor.
void emlist_set_be_flag_disp(int x, int y, int flag)
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    u32 i;
    u8 bit;
    int col;
    int first;

    if (flag) {
        eprintf(x, y, 0, 0, ">");
    }
    x += 8;
    bit = 0x80;
    for (i = 0; i <= 7; i++) {
        int c = 0;
        if (flag) {
            c = (i == EmList.wk->subNo) ? 4 : 0;
        }
        if (p->flags & bit) {
            eprintf(x, y, c, 0, "1");
        } else {
            eprintf(x, y, c, 0, "0");
        }
        bit >>= 1;
        x += 8;
        if ((i & 3) == 3) {
            x += 8;
        }
    }
    x += 0x10;
    bit = 0x80;
    col = flag ? 4 : 0;
    first = 0;
    for (i = 0; i <= 7; i++) {
        if (p->flags & bit) {
            if (first) {
                eprintf(x, y, col, 0, ",%s", be_flag_name[i]);
            } else {
                first = 1;
                eprintf(x, y, col, 0, ":%s", be_flag_name[i]);
            }
            x += (strlen(be_flag_name[i]) + 1) * 8;
        }
        bit >>= 1;
    }
    x += 0x10;
    if (flag) {
        char* name = be_flag_name[EmList.wk->subNo];
        if (name != NULL) {
            eprintf(x, y, col, 0, "[%s]", name);
        }
    }
}

// TYPE field text: hex / decimal type and its name from the enemy's type table.
void emlist_set_type_disp(int x, int y, int flag)
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    int col;

    if (flag) {
        eprintf(x, y, 0, 0, ">");
    }
    x += 8;
    col = flag ? 4 : 0;
    eprintf(x, y, col, 0, "0x%02x, %03d", p->type, p->type);
    if (p->id != 0) {
        x += 0x60;
        if (emlist_get_numof_str(EmListIdTbl[p->id].type) > p->type) {
            eprintf(x, y, col, 0, ":%s", EmListIdTbl[p->id].type[p->type]);
        }
    }
}

// SET field text: set byte and its name.
void emlist_set_set_disp(int x, int y, int flag)
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    int col;

    if (flag) {
        eprintf(x, y, 0, 0, ">");
    }
    x += 8;
    col = flag ? 4 : 0;
    eprintf(x, y, col, 0, "0x%02x, %03d", p->set, p->set);
    if (p->id != 0) {
        x += 0x60;
        if (emlist_get_numof_str(EmListIdTbl[p->id].set) > p->set) {
            eprintf(x, y, col, 0, ":%s", EmListIdTbl[p->id].set[p->set]);
        }
    }
}

// The 32 em flag bits as 0/1 digits and the name of the bit under the cursor.
void emlist_set_em_flag_disp(int x, int y, int flag)
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    u32 i;
    u32 bit;
    int col;

    if (flag) {
        eprintf(x, y, 0, 0, ">");
    }
    x += 8;
    bit = 0x80000000;
    for (i = 0; i <= 31; i++) {
        col = 0;
        if (flag) {
            col = (i == EmList.wk->subNo) ? 4 : 0;
        }
        if (p->flags4 & bit) {
            eprintf(x, y, col, 0, "1");
        } else {
            eprintf(x, y, col, 0, "0");
        }
        bit >>= 1;
        x += 8;
        if ((i & 7) == 7) {
            x += 8;
        }
    }
    if (flag && p->id != 0) {
        if (emlist_get_numof_str(EmListIdTbl[p->id].flag) > 31 - EmList.wk->subNo) {
            eprintf(x, y, col, 0, ":%s", EmListIdTbl[p->id].flag[31 - EmList.wk->subNo]);
        }
    }
}

// CHARA field text: character byte and its name.
void emlist_set_char_disp(int x, int y, int flag)
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    int col;

    if (flag) {
        eprintf(x, y, 0, 0, ">");
    }
    x += 8;
    col = flag ? 4 : 0;
    eprintf(x, y, col, 0, "0x%02x, %03d", p->Character, p->Character);
    if (p->id != 0) {
        x += 0x60;
        if (emlist_get_numof_str(EmListIdTbl[p->id].chr) > p->Character) {
            eprintf(x, y, col, 0, ":%s", EmListIdTbl[p->id].chr[p->Character]);
        }
    }
}

// HP field text.
void emlist_set_hp_disp(int x, int y, int flag)
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);

    if (flag) {
        eprintf(x, y, 0, 0, ">");
    }
    x += 8;
    eprintf(x, y, flag ? 4 : 0, 0, "%d", p->hp);
}

// Guard R field text (metres).
void emlist_set_guard_r_disp(int x, int y, int flag)
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);

    if (flag) {
        eprintf(x, y, 0, 0, ">");
    }
    x += 8;
    eprintf(x, y, flag ? 4 : 0, 0, "%d m", p->Guard_r);
}

// Writes the list to both hosts.
void emlist_file_save(int no)
{
    char name[0x100];

    EmList.wk->fileNo = no + 1;
    emlist_set_fname(name, no, 0);
    HDWrite(name, pG->Em_list, 0x1FE0);
    emlist_set_fname(name, no, 1);
    HDWrite(name, pG->Em_list, 0x1FE0);
}

// Reads ESL file `no` of the room from the host straight into pG->Em_list; 0 when missing.
int emlist_file_load(int no)
{
    char name[0x100];
    int ret;

    EmList.wk->fileNo = no + 1;
    emlist_set_fname(name, no, 0);
    ret = HDRead(name, pG->Em_list);
    if (ret == 0) {
        memclr_asm(pG->Em_list, 0x1FE0);
        return 0;
    }
    return ret;
}

// "<host><dir>/emlist%02x.esl": file `no` = dir no / 10, index no % 10.
void emlist_set_fname(char* buf, int no, int mode)
{
    char dir[3][0x20] = {"/data/etc/emlist", "/data/etc/emleon", "/data/etc/omake"};
    int kind = no / 10;

    no %= 10;
    switch (mode) {
    case 0:
    default:
        sprintf(buf, "%s%s%02x.esl", "x:\\soft", dir[kind], no);
        break;
    case 1:
        sprintf(buf, "%s%s%02x.esl", "d:\\bio4", dir[kind], no);
        break;
    }
}

// Draws every entry of the current room as a direction arrow (blinking for the selected one, set
// entries in green) plus its guard radius cylinder.
void emlist_EmDir_disp()
{
    Mtx m;
    Vec pos;
    Vec rot;
    u8 fill[4] = {0x66, 0x00, 0x00, 0xFF};
    u8 line[4] = {0xC0, 0x40, 0x40, 0xFF};
    int i;
    u8 blink;

    blink = pG->Frame_cnt & 0xF;
    if (pG->Frame_cnt & 0x10) {
        blink = 15 - blink;
    }
    blink *= 3;
    for (i = 0; i <= 0xFE; i++) {
        u32 ofs = i * 0x20 + PG_OFS(Em_list);
        EmListEnt* p = (EmListEnt*) ((u8*) pG + ofs);
        if (p->id == 0) {
            continue;
        }
        if (pG->stage_no != p->room >> 8 || pG->room_no != (p->room & 0xFF)) {
            continue;
        }
        if (*((u8*) pG + ofs) & 1) {
            if (i == EmList.wk->listNo) {
                ((GXColor*) fill)->r = 0x00;
                ((GXColor*) fill)->g = 0x40;
                ((GXColor*) fill)->b = 0x00;
                ((GXColor*) fill)->a = 0xFF;
                ((GXColor*) line)->r = 0x81;
                ((GXColor*) line)->g = 0xC0;
                ((GXColor*) line)->b = 0x40;
                ((GXColor*) line)->a = 0xFF;
                ((GXColor*) fill)->r += blink;
                ((GXColor*) fill)->g += blink;
                ((GXColor*) fill)->b += blink;
            } else {
                ((GXColor*) fill)->r = 0x00;
                ((GXColor*) fill)->g = 0x0C;
                ((GXColor*) fill)->b = 0x00;
                ((GXColor*) fill)->a = 0xFF;
                ((GXColor*) line)->r = 0x20;
                ((GXColor*) line)->g = 0x80;
                ((GXColor*) line)->b = 0x20;
                ((GXColor*) line)->a = 0xFF;
            }
        } else {
            if (i == EmList.wk->listNo) {
                ((GXColor*) fill)->r = 0x40;
                ((GXColor*) fill)->g = 0x40;
                ((GXColor*) fill)->b = 0x40;
                ((GXColor*) fill)->a = 0xFF;
                ((GXColor*) line)->r = 0x80;
                ((GXColor*) line)->g = 0x80;
                ((GXColor*) line)->b = 0x80;
                ((GXColor*) line)->a = 0xFF;
                ((GXColor*) fill)->r += blink;
                ((GXColor*) fill)->g += blink;
                ((GXColor*) fill)->b += blink;
            } else {
                ((GXColor*) fill)->r = 0x0C;
                ((GXColor*) fill)->g = 0x0C;
                ((GXColor*) fill)->b = 0x0C;
                ((GXColor*) fill)->a = 0xFF;
                ((GXColor*) line)->r = 0x40;
                ((GXColor*) line)->g = 0x40;
                ((GXColor*) line)->b = 0x40;
                ((GXColor*) line)->a = 0xFF;
            }
        }
        rot.x = (f32) (s16) p->rot[0] * (3.1415927f / 16384.0f);
        rot.y = (f32) (s16) p->rot[1] * (3.1415927f / 16384.0f);
        rot.z = (f32) (s16) p->rot[2] * (3.1415927f / 16384.0f);
        pos.x = (f32) (s16) p->pos[0] * 10.0f;
        pos.y = (f32) (s16) p->pos[1] * 10.0f;
        pos.z = (f32) (s16) p->pos[2] * 10.0f;
        RotMatrix(m, &rot);
        TransMatrix(m, &pos);
        TprimDraw3D(1);
        TprimDrawMtxDirection(m, (GXColor*) fill, (GXColor*) line);
        if (i == EmList.wk->listNo) {
            blink = pG->Frame_cnt & 0xF;
            if (pG->Frame_cnt & 0x10) {
                blink = 15 - blink;
            }
            blink <<= 3;
            Draw_cylinder(&pos, (f32) (s16) p->Guard_r * 1000.0f, 500.0f,
                          0x404040FF + (blink << 24) + (blink << 16) + (blink << 8));
        } else {
            Draw_cylinder(&pos, (f32) (s16) p->Guard_r * 1000.0f, 500.0f, 0x404040FF);
        }
    }
}

// Entry of the current room nearest to the screen cursor (within 30 pixels); the cursor snaps to it.
// Returns the entry index or -1.
int emlist_catch_em()
{
    Vec pos;
    f32 scr[3];
    f32 hit[2];
    f32 min = 900.0f;
    int found = -1;
    u32 i;

    for (i = 0; i <= 0xFE; i++) {
        EmListEnt* p = EMLIST_ENT(i);
        f32 dist;
        if (p->id == 0) {
            continue;
        }
        if (pG->stage_no != p->room >> 8 || pG->room_no != (p->room & 0xFF)) {
            continue;
        }
        pos.x = (f32) (s16) p->pos[0] * 10.0f;
        pos.y = (f32) (s16) p->pos[1] * 10.0f;
        pos.z = (f32) (s16) p->pos[2] * 10.0f;
        TutilGetScreenPos(&pos, scr, 0);
        dist = (scr[0] - EmList.wk->cursorX) * (scr[0] - EmList.wk->cursorX) +
               (scr[1] - EmList.wk->cursorY) * (scr[1] - EmList.wk->cursorY);
        if (!(dist > min)) {
            hit[0] = scr[0];
            min = dist;
            hit[1] = scr[1];
            found = i;
        }
    }
    if (found == -1) {
        return -1;
    }
    EmList.wk->cursorX = hit[0];
    EmList.wk->cursorY = hit[1];
    return found;
}

// Moves the entry to the cursor's ground position, clamped to the 16-bit cm range.
extern "C" void emlist_em_move_to_cursor__Fv(EmListEnt* p)
{
    Vec pos;
    Vec cur;

    cur.x = (f32) (s16) p->pos[0] * 10.0f;
    cur.y = (f32) (s16) p->pos[1] * 10.0f;
    cur.z = (f32) (s16) p->pos[2] * 10.0f;
    Get3DPosFrom2D(&pos, EmList.wk->cursorX, EmList.wk->cursorY, cur.y);
    if (pos.x > 327670.0f) {
        pos.x = 327670.0f;
    }
    if (pos.x < -327670.0f) {
        pos.x = -327670.0f;
    }
    if (pos.z > 327670.0f) {
        pos.z = 327670.0f;
    }
    if (pos.z < -327670.0f) {
        pos.z = -327670.0f;
    }
    p->pos[0] = (s16) (pos.x * 0.1f);
    p->pos[2] = (s16) (pos.z * 0.1f);
}

// Number of entries before the "END" terminator (0 without a table or terminator).
int emlist_get_numof_str(const char** tbl)
{
    int i;

    if (tbl == NULL) {
        return 0;
    }
    for (i = 0; i < 256; i++) {
        if (tbl[i][0] == 'E' && tbl[i][1] == 'N' && tbl[i][2] == 'D') {
            return i;
        }
    }
    return 0;
}

// Copies the pad into the work; START toggles the free camera (camMode), which then eats the pad.
void emlistCameraMove()
{
    JOY_COPY(EmList.wk, 0x178, 0);
    if (Joy[0].trg & JOY_START) {
        EmList.wk->camMode ^= 1;
        EmList.wk->joy.trg = 0;
        EmList.wk->joy.on = 0;
        EmList.wk->joy.rep = 0;
        EmList.wk->joy.rep2 = 0;
    }
    if (EmList.wk->camMode != 0) {
        CamDbg.move(&pG->Camera, &Joy[0], 0);
        EmList.wk->joy.trg = 0;
        EmList.wk->joy.on = 0;
        EmList.wk->joy.rep = 0;
        EmList.wk->joy.rep2 = 0;
        DbgFlagOn(pG, DBG_DBG_CAM);
        if (pG->Frame_cnt & 0x10) {
            eprintf(0x140, 0x18, 4, 0, "1P CAMERA MODE");
        }
        EmList.wk->cursorX = (Screen.x + Screen.width) * 0.5f;
        EmList.wk->cursorY = (Screen.y + Screen.height) * 0.5f;
    }
}

// Aims the camera at the current entry (keeping the camera offset) unless it is already on screen.
void emlistCamToPoin()
{
    Vec d;
    Vec pos;
    Vec scr;
    Camera* cam = &pG->Camera;
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);

    if (p->id != 0 && pG->stage_no == p->room >> 8 && pG->room_no == (p->room & 0xFF)) {
        Vec tmp;
        pos.x = (f32) (s16) p->pos[0] * 10.0f;
        pos.y = (f32) (s16) p->pos[1] * 10.0f;
        pos.z = (f32) (s16) p->pos[2] * 10.0f;
        tmp = pos;
        if (GetScreenPos(&tmp, &scr) != 0 && scr.x > 50.0f && scr.x < 462.0f && scr.y > 100.0f && scr.y < 348.0f) {
            return;
        }
        PSVECSubtract(&cam->param.pos, &cam->param.at, &d);
        EmList.wk->cam.param.at = pos;
        PSVECAdd(&EmList.wk->cam.param.at, &d, &EmList.wk->cam.param.pos);
        EmList.wk->cam.Up.x = 0.0f;
        EmList.wk->cam.Up.y = 1.0f;
        EmList.wk->cam.Up.z = 0.0f;
        EmList.wk->cam.Distance =
            VEC_DIST(&EmList.wk->cam.param.pos, &EmList.wk->cam.param.at);
        EmList.wk->cam.param.fovy = cam->param.fovy;
        CameraSetOrientationUp(&EmList.wk->cam);
        CamCtrl.SetExtraCamera(&EmList.wk->cam);
        cam->param.at = EmList.wk->cam.param.at;
        cam->param.pos = EmList.wk->cam.param.pos;
        EmList.wk->cursorX = (Screen.x + Screen.width) * 0.5f;
        EmList.wk->cursorY = (Screen.y + Screen.height) * 0.5f;
    }
}

// Puts the screen cursor on the current entry (if it is in this room).
void emlistCursorToTarget()
{
    EmListEnt* p = EMLIST_ENT(EmList.wk->listNo);
    Vec pos;
    f32 scr[3];

    if (p->id != 0 && pG->stage_no == p->room >> 8 && pG->room_no == (p->room & 0xFF)) {
        pos.x = (f32) (s16) p->pos[0] * 10.0f;
        pos.y = (f32) (s16) p->pos[1] * 10.0f;
        pos.z = (f32) (s16) p->pos[2] * 10.0f;
        TutilGetScreenPos(&pos, scr, 0);
        EmList.wk->cursorX = scr[0];
        EmList.wk->cursorY = scr[1];
    }
}
