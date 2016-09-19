#include "types.h"
#include "defines.h"
#include "battle_locations.h"
#include "battle_structs.h"
#include "vanilla_functions.h"
#include "new_battle_struct.h"
#include "static_references.h"

u8 weather_abilities_effect();
u8 check_mega_condition(u8 bank);
u8 find_move_in_table(u16 move, u16 table_ptr[]);

u8 is_bank_present(u8 bank)
{
    if(absent_bank_flags & bits_table[bank])
        return 0;
    return 1;
}

u32 two_options_rand(u32 option1, u32 option2);
u8 get_attacking_move_type();
u8 check_ability(u8 bank, u8 ability);
u16 get_speed(u8 bank);

u8 get_target_of_move(u16 move, u8 target_given, u8 adjust)
{
    u8 target_case;
    u8 old_target = battle_stuff_ptr.ptr->move_target[bank_attacker];
    u8 result_target = old_target;
    u8 attacker_side = is_bank_from_opponent_side(bank_attacker);
    u8 target_side = attacker_side ^ 1;
    if (target_given)
        target_case = target_given;
    else
        target_case = move_table[move].target;
    if(battle_flags.double_battle)
    {
        switch (target_case)
        {
            case 4:
            case 0: //chosen target
                if (side_timers[target_side].followme_timer && battle_participants[side_timers[target_side].followme_target].current_hp)
                    result_target = side_timers[target_side].followme_target;
                else
                {
                    u8 ally_bank=bank_attacker^2;
                    u8 ally_alive=is_bank_present(ally_bank);
                    u8 target_alive=is_bank_present(target_side);
                    u8 target_partner=target_side^2;
                    u8 target_partner_alive=is_bank_present(target_partner);
                    if(adjust && target_case==0)
                    {
                        if(old_target==ally_bank && !is_bank_present(ally_bank))
                            result_target=target_side;
                        if(result_target==target_side && !is_bank_present(target_side))
                            result_target^=2;
                    }
                    else
                    {
                        if(target_alive && !target_partner_alive)
                            result_target=target_side;
                        else if (!target_alive && target_partner_alive)
                            result_target=target_partner;
                        else
                            result_target=two_options_rand(target_side,target_partner);
                    }
                    u16 move_type = get_attacking_move_type();
                    u8 ability = 0;
                    u8 redirect = 0;
                    u16 max_speed=0;
                    u8 redirect_candidate=0;
                    if (move_type == TYPE_ELECTRIC)
                        ability = ABILITY_LIGHTNING_ROD;
                    else if (move_type == TYPE_WATER)
                        ability = ABILITY_STORM_DRAIN;
                    u8 alive_candidates = target_alive+ally_alive+target_partner_alive;
                    if(ability && check_ability(target_side,ability) && target_alive && max_speed<get_speed(target_side))
                    {
                        redirect_candidate=target_side;
                        max_speed=get_speed(target_side);
                        redirect=1;
                    }
                    if(ability && check_ability(target_partner,ability) && target_partner_alive && max_speed<get_speed(target_partner))
                    {
                        redirect_candidate=target_partner;
                        max_speed=get_speed(target_partner);
                        redirect=1;
                    }
                    if(ability && check_ability(ally_bank,ability) && ally_alive && max_speed<get_speed(ally_bank))
                    {
                        redirect_candidate=ally_bank;
                        redirect=1;
                    }
                    if(redirect && alive_candidates>=2 && (!adjust || (old_target!=redirect_candidate)))
                    {
                        result_target=redirect_candidate;
                        record_usage_of_ability(result_target, ability);
                        special_statuses[result_target].lightning_rod_redirected = 1;
                        if (ability == ABILITY_STORM_DRAIN)
                            new_battlestruct.ptr->various.stormdrain = 1;
                    }
                }
                break;
            case 1:
            case 8:
            case 64:
                result_target = target_side;
                if (absent_bank_flags & bits_table[result_target])
                    result_target ^= 2;
                break;
            case 32:
                result_target=0;
                while((result_target==bank_attacker || !is_bank_present(result_target)) && result_target<no_of_all_banks)
                    result_target++;
                break;
            case 2: //user
                if(current_move==MOVE_ACUPRESSURE && is_bank_present(bank_attacker^2))
                {
                    if(adjust)
                        break;
                    if(rng()&2)
                    {
                        result_target = bank_attacker^2;
                        break;
                    }
                }
            case 16:
                result_target = bank_attacker;
                break;
        }
    }
    else
    {
        if(!adjust)
        {
            switch(target_case)
            {
                case 0:
                case 1:
                case 4:
                case 8:
                case 32:
                case 64:
                    result_target = target_side;
                    break;
                case 2:
                case 16:
                    result_target = bank_attacker;
                    break;
            }
        }
    }
    battle_stuff_ptr.ptr->move_target[bank_attacker] = result_target;
    return result_target;
}

void set_attacking_move_type()
{
    u8 ability=battle_participants[bank_attacker].ability_id;
    u8 move_type = TYPE_EGG;
    if(new_battlestruct.ptr->bank_affecting[bank_attacker].electrify)
        move_type=TYPE_ELECTRIC;
    else
    {
        switch(current_move)
        {
        case MOVE_WEATHER_BALL:
            if(weather_abilities_effect())
            {
                if (battle_weather.flags.sun || battle_weather.flags.permament_sun || battle_weather.flags.harsh_sun)
                {
                    move_type=TYPE_FIRE;
                    battle_scripting.damage_multiplier=2;
                }
                else if (battle_weather.flags.rain || battle_weather.flags.permament_rain ||
                          battle_weather.flags.heavy_rain || battle_weather.flags.downpour)
                {
                    move_type=TYPE_WATER;
                    battle_scripting.damage_multiplier=2;
                }
                else if (battle_weather.flags.hail || battle_weather.flags.permament_hail)
                {
                    move_type=TYPE_ICE;
                    battle_scripting.damage_multiplier=2;
                }
                else if (battle_weather.flags.sandstorm || battle_weather.flags.permament_sandstorm)
                {
                    move_type=TYPE_ROCK;
                    battle_scripting.damage_multiplier=2;
                }
            }
            break;
        case MOVE_HIDDEN_POWER:
            {
                struct iv_set *ivs= &battle_participants[bank_attacker].ivs;
                u32 sum=((ivs->iv_hp)&1)+(((ivs->iv_atk)&1)<<1)+(((ivs->iv_def)&1)<<2)+(((ivs->iv_spd)&1)<<3)
                       +(((ivs->iv_sp_atk)&1)<<4)+(((ivs->iv_sp_def)&1)<<5);
                move_type=__udivsi3(sum*15,63);
            }
            break;
        default:
            break;
        }
        u8 ate=0;
        if(move_type == TYPE_EGG && (DAMAGING_MOVE) && has_ability_effect(bank_attacker,0,1))
        {
            switch(ability)
            {
            case ABILITY_AERILATE:
                move_type=TYPE_FLYING;
                ate=1;
                break;
            case ABILITY_PIXILATE:
                move_type=TYPE_FAIRY;
                ate=1;
                break;
            case ABILITY_REFRIGERATE:
                move_type=TYPE_ICE;
                ate=1;
                break;
            }
        }
        if(ate==1 && DAMAGING_MOVE)
            new_battlestruct.ptr->various.ate_bonus=1;
    }
    if((new_battlestruct.ptr->field_affecting.ion_deluge)
       && (move_table[current_move].type==TYPE_NORMAL || check_ability(bank_attacker,ABILITY_NORMALIZE)))
        move_type = TYPE_ELECTRIC;

    if (move_type != TYPE_EGG)
    {
        battle_stuff_ptr.ptr->dynamic_move_type=move_type + 0x80;
    }
    else
        battle_stuff_ptr.ptr->dynamic_move_type = 0;
}


void clear_move_outcome()
{
    *(u8 *)(0x202427C)=0;
}

void* get_move_battlescript_ptr(u16 move)
{
    u32* ptr_to_movesciprts_table = (void*) 0x0803E854;
    u32* ptr_to_movescript = (void*) *ptr_to_movesciprts_table + move_table[move].script_id * 4;
    return (void*) *ptr_to_movescript;
}

u16 get_mega_species(u16 species) //0xFB = mega evo, 0xFC = primal reversion, 0xFD = raquaza mego evo, 0xFF = revert to normal state
{
    struct evolution_data *evolution_table= (void *)(evo_table_ptr_ptr);
    for (u8 i = 0; i < NUM_OF_EVOS; i++)
    {
        u8 method = evolution_table->poke_evolutions[species].evos[i].method;
        if (method == 0xFB ||  method == 0xFC)
        {
            return evolution_table->poke_evolutions[species].evos[i].poke;
        }
    }
    return 0;
}

u8 mega_evolution_script[] = {0x83, 88, 0, 0x10, 0x05, 0x02, 0x45, 1, 0x1E, 0, 0, 0, 0, 0x3A, 0x83, 89, 0, 0x10, 0x04, 0x02, 0x83, 0x0, 0x0, 0x3F};
u8 fervent_evolution_script[] = {0x83, 88, 0, 0x10, 0x06, 0x02, 0x45, 1, 0x1E, 0, 0, 0, 0, 0x3A, 0x83, 89, 0, 0x10, 0x04, 0x02, 0x83, 0x0, 0x0, 0x3F};

u8 is_multi_battle();

void reset_indicators_height()
{
    struct mega_related* mega = &new_battlestruct.ptr->mega_related;
    for(u8 i = 0; i<no_of_all_banks ;i++)
    {
        objects[mega->indicator_id_pbs[i]].pos2.y=0;
    }
}

u8 check_mega_evo(u8 bank)
{
    struct battle_participant* attacker_struct = &battle_participants[bank];
    u16 mega_species = get_mega_species(attacker_struct->poke_species);
    u8 banks_side = is_bank_from_opponent_side(bank);
    u8 can_mega_evolve=0;
    if(bank==0)
    {
        can_mega_evolve=new_battlestruct.ptr->mega_related.user_trigger;
        if(can_mega_evolve)
        {
            if(!is_multi_battle())
            {
                new_battlestruct.ptr->mega_related.evo_happened_pbs|=0x5;
            }
            else
            {
                new_battlestruct.ptr->mega_related.evo_happened_pbs|=0x1;
            }
            objects[new_battlestruct.ptr->mega_related.trigger_id].private[ANIM_STATE]=DISABLE;
        }
    }
    else if(bank==2 && !is_multi_battle())
    {
        can_mega_evolve=new_battlestruct.ptr->mega_related.ally_trigger;
        if(can_mega_evolve)
        {
            new_battlestruct.ptr->mega_related.evo_happened_pbs|=0x5;
            objects[new_battlestruct.ptr->mega_related.trigger_id].private[ANIM_STATE]=DISABLE;
        }
    }
    else
    {
        can_mega_evolve=check_mega_condition(bank);
        if(can_mega_evolve)
        {
            (new_battlestruct.ptr->mega_related.evo_happened_pbs)|=bits_table[bank];
        }
    }
    if(mega_species && can_mega_evolve)
    {
        struct pokemon* poke_address;
        if (banks_side == 1)
        {
            poke_address = &party_opponent[battle_team_id_by_side[bank]];
            new_battlestruct.ptr->mega_related.ai_party_mega_check|=bits_table[battle_team_id_by_side[bank]];
        }
        else
        {
            poke_address = &party_player[battle_team_id_by_side[bank]];
            new_battlestruct.ptr->mega_related.party_mega_check|=bits_table[battle_team_id_by_side[bank]];
        }
        set_attributes(poke_address, ATTR_SPECIES, &mega_species);
        calculate_stats_pokekmon(poke_address);
        attacker_struct->atk = get_attributes(poke_address, ATTR_ATTACK, 0);
        attacker_struct->def = get_attributes(poke_address, ATTR_DEFENCE, 0);
        attacker_struct->spd = get_attributes(poke_address, ATTR_SPEED, 0);
        attacker_struct->sp_atk = get_attributes(poke_address, ATTR_SPECIAL_ATTACK, 0);
        attacker_struct->sp_def = get_attributes(poke_address, ATTR_SPECIAL_DEFENCE, 0);
        attacker_struct->poke_species = mega_species;
        struct basestat_data *pokemon_table = (void *)(poke_stat_table_ptr_ptr);
        attacker_struct->type1 = pokemon_table->poke_stats[mega_species].type1;
        attacker_struct->type2 = pokemon_table->poke_stats[mega_species].type2;
        // The ability 1 and ability 2 of the species mega in the base stat table should both be set and
        // have the same value.
        attacker_struct->ability_id = pokemon_table->poke_stats[mega_species].ability1;

        attacker_struct->max_hp = get_attributes(poke_address, ATTR_TOTAL_HP, 0);
        attacker_struct->current_hp = get_attributes(poke_address, ATTR_CURRENT_HP, 0);
        attacker_struct->level = get_attributes(poke_address, ATTR_LEVEL, 0);

        //set buffer for new species and item used
        battle_text_buff1[0] = 0xFD;
        battle_text_buff1[1] = 6;
        battle_text_buff1[2] = mega_species;
        battle_text_buff1[3] = mega_species >> 8;
        battle_text_buff1[4] = 0xFF;
        last_used_item = attacker_struct->held_item;

        //buffer for mega ring
        u16 MEGA_RING = KEYSTONE;
        battle_text_buff2[0] = 0xFD;
        battle_text_buff2[1] = 10;
        battle_text_buff2[2] = MEGA_RING;
        battle_text_buff2[3] = MEGA_RING >> 8;
        battle_text_buff2[4] = 0xFF;

        if(can_mega_evolve==2)
        {
            execute_battle_script(&fervent_evolution_script);
        }
        else
        {
            execute_battle_script(&mega_evolution_script);
        }

        new_battlestruct.ptr->various.active_bank = bank;
        return 1;
    }
    else
        return 0;
}

u16 moveshitting_onair[] = {MOVE_TWISTER, MOVE_SKY_UPPERCUT, MOVE_WHIRLWIND, MOVE_GUST, MOVE_THUNDER, MOVE_HURRICANE, MOVE_SMACK_DOWN, MOVE_THOUSAND_ARROWS, 0xFFFF};
u16 moveshitting_underground[] = {MOVE_MAGNITUDE, MOVE_EARTHQUAKE, MOVE_FISSURE, 0xFFFF};
u16 moveshitting_underwater[] = {MOVE_SURF, MOVE_WHIRLPOOL, 0xFFFF};

void bs_start_attack()
{
    u8 mode=0;  //mode 0 - get type with adjusting chosen target
                //mode 1 - get type with calculating target from scratch
                //mode 2 - don't get type and calculating target from scratch

    for (u8 i = 0; i < no_of_all_banks; i++)
    {
        bank_attacker = i;
        if (menu_choice_pbs[i] == 0)
        {
            if (check_mega_evo(i))
                return;
        }
    }
    bank_attacker=turn_order[current_move_turn];
    if(!(bits_table[bank_attacker]&battle_stuff_ptr.ptr->absent_bank_flags_prev_turn))
    {
        struct battle_participant *attacker_struct = &battle_participants[bank_attacker];
        crit_loc=1;
        battle_scripting.damage_multiplier=0;
        battle_stuff_ptr.ptr->atk_canceller_state_tracker=0;
        clear_move_outcome();
        multihit_counter=0;
        battle_communication_struct.multistring_chooser=0;
        current_move_position=battle_stuff_ptr.ptr->chosen_move_position[bank_attacker];
        if(protect_structs[bank_attacker].flag0_onlystruggle)
        {
            protect_structs[bank_attacker].flag0_onlystruggle=0;
            current_move=MOVE_STRUGGLE;
            last_used_move=MOVE_STRUGGLE;
            hitmarker|=HITMARKER_NO_PPDEDUCT;
            mode=2;
        }
        else
        {
            if(attacker_struct->status2.multiple_turn_move || attacker_struct->status2.recharge)
            {
                current_move=locked_move[bank_attacker];
                last_used_move=locked_move[bank_attacker];
            }
            else
            {
                u16 *enc_move=&disable_structs[bank_attacker].encored_move;
                u8* enc_idx=&disable_structs[bank_attacker].encored_index;
                u16 *atk_enc_move=&(attacker_struct->moves[*enc_idx]);
                if(*enc_move)
                {
                    if(*atk_enc_move==*enc_move)
                    {
                        current_move=*enc_move;
                        last_used_move=*enc_move;
                        current_move_position=*enc_idx;
                        mode=1;
                    }
                    else
                    {
                        current_move_position=*enc_idx;
                        current_move=*atk_enc_move;
                        last_used_move=*atk_enc_move;
                        *enc_move=0;
                        *enc_idx=0;
                        disable_structs[bank_attacker].encore_timer&=0xF0;
                        mode=1;
                    }
                }
                else
                {
                    current_move=chosen_move_by_banks[bank_attacker];
                    last_used_move=chosen_move_by_banks[bank_attacker];
                    if(chosen_move_by_banks[bank_attacker]!=attacker_struct->moves[current_move_position])
                        mode=1;
                }
            }
        }
        if (mode==0)
        {
            set_attacking_move_type();
            bank_target=get_target_of_move(current_move,0,1);
        }
        else
        {
            if (mode==1)
                set_attacking_move_type();
            bank_target=get_target_of_move(current_move,0,0);
        }

        battlescripts_curr_instruction = get_move_battlescript_ptr(current_move);
        battle_state_mode=0xA;
        if (current_move == last_used_moves[bank_attacker])
        {
            if (new_battlestruct.ptr->bank_affecting[bank_attacker].same_move_used < 5)
                new_battlestruct.ptr->bank_affecting[bank_attacker].same_move_used++;
        }
        else
            new_battlestruct.ptr->bank_affecting[bank_attacker].same_move_used = 0;

        if (find_move_in_table(current_move, &moveshitting_onair[0]))
            hitmarker |= HITMARKER_IGNORE_ON_AIR;
        else if (find_move_in_table(current_move, &moveshitting_underground[0]))
            hitmarker |= HITMERKER_IGNORE_UNDERGROUND;
        else if (find_move_in_table(current_move, &moveshitting_underwater[0]))
            hitmarker |= HITMARKER_IGNORE_UNDERWATER;
    }
    else
        battle_state_mode=0xC;
}

s8 get_bracket_alteration_factor(u8 bank, u8 item_effect);

void bc_preattacks()
{
    u8 play_script=0;

    u8 *count = &(battle_stuff_ptr.ptr->pre_attacks_bank_counter);
    if(!(hitmarker&HITMARKER_FLEE))
    {   reset_indicators_height();
        while(*count<no_of_all_banks)
        {   bank_attacker=*count;
            (*count)++;
            if(menu_choice_pbs[bank_attacker]==0)
            {
                u8 item_effect=get_item_effect(bank_attacker,1);
                s8 alteration_occurs=get_bracket_alteration_factor(bank_attacker,item_effect);
                if(alteration_occurs==1)
                {
                    if(item_effect==ITEM_EFFECT_QUICKCLAW)
                        call_bc_move_exec(&quickclaw_bs);
                    else if(item_effect==ITEM_EFFECT_CUSTAPBERRY)
                        call_bc_move_exec(&custapberry_bs);
                    play_script=1;
                    last_used_item=battle_participants[bank_attacker].held_item;
                    break;
                }
            }
        }
    }
    if(!play_script)
    {
        clear_atk_up_if_hit_flag_unless_enraged();
        current_move_turn=0;
        battle_state_mode=battle_state_mode_first_assigned;
        dynamic_base_power=0;
        battle_stuff_ptr.ptr->dynamic_move_type=0;
        b_c=&bc_bs_executer;
        battle_communication_struct.move_effect=0;
        battle_communication_struct.field4=0;
        battle_scripting.field16=0;
        battle_resources.ptr->battlescript_stack->stack_height=0;
    }
}
