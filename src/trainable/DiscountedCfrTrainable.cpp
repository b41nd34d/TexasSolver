//
// Created by Xuefeng Huang on 2020/1/31.
//

#include "include/trainable/DiscountedCfrTrainable.h"
#include <sstream>

//#define DEBUG;

DiscountedCfrTrainable::DiscountedCfrTrainable(vector<PrivateCards> *privateCards,
                                               ActionNode &actionNode) : action_node(actionNode) {
    this->privateCards = privateCards;
    this->action_number = action_node.getChildrens().size();
    this->card_number = privateCards->size();

    this->evs = vector<float>(this->action_number * this->card_number,0.0);
    this->r_plus = vector<float>(this->action_number * this->card_number,0.0);
    this->r_plus_sum = vector<float>(this->card_number,0.0);

    this->cum_r_plus = vector<float>(this->action_number * this->card_number,0.0);
}

bool DiscountedCfrTrainable::isAllZeros(const vector<float>& input_array) {
    for(float i:input_array){
        if (i != 0)return false;
    }
    return true;
}

const vector<float> DiscountedCfrTrainable::getAverageStrategy() {
    vector<float> average_strategy;
    average_strategy = vector<float>(this->action_number * this->card_number);
    for (int private_id = 0; private_id < this->card_number; private_id++) {
        float r_plus_sum = 0;
        for (int action_id = 0; action_id < action_number; action_id++) {
            int index = action_id * this->card_number + private_id;
            r_plus_sum += this->cum_r_plus[index];
        }

        for (int action_id = 0; action_id < action_number; action_id++) {
            int index = action_id * this->card_number + private_id;
            if(r_plus_sum) {
                average_strategy[index] = this->cum_r_plus[index] / r_plus_sum;
            }else{
                average_strategy[index] = 1.0 / this->action_number;
            }
        }
    }
    return average_strategy;
}

const vector<float> DiscountedCfrTrainable::getcurrentStrategy() {
    return this->getcurrentStrategyNoCache();
}

void DiscountedCfrTrainable::copyStrategy(shared_ptr<Trainable> other_trainable){
    shared_ptr<DiscountedCfrTrainable> trainable = dynamic_pointer_cast<DiscountedCfrTrainable>(other_trainable);
    this->r_plus.assign(trainable->r_plus.begin(),trainable->r_plus.end());
    this->cum_r_plus.assign(trainable->cum_r_plus.begin(),trainable->cum_r_plus.end());
}

const vector<float> DiscountedCfrTrainable::getcurrentStrategyNoCache() {
    vector<float> current_strategy;
    current_strategy = vector<float>(this->action_number * this->card_number);
    if(this->r_plus_sum.empty()){
        fill(current_strategy.begin(),current_strategy.end(),1.0 / this->action_number);
    }else {
        for (int action_id = 0; action_id < action_number; action_id++) {
            for (int private_id = 0; private_id < this->card_number; private_id++) {
                int index = action_id * this->card_number + private_id;
                if(this->r_plus_sum[private_id] != 0) {
                    current_strategy[index] = max(float(0.0),this->r_plus[index]) / this->r_plus_sum[private_id];
                }else{
                    current_strategy[index] = 1.0 / (this->action_number);
                }
#ifdef DEBUG
                if(this->r_plus[index] != this->r_plus[index]) throw runtime_error("nan found");
#endif
            }
        }
    }
    return current_strategy;
}

void DiscountedCfrTrainable::setEv(const vector<float>& evs){
    if(evs.size() != this->evs.size()) throw runtime_error("size mismatch in discountcfrtrainable setEV");
    for(std::size_t i = 0;i < evs.size();i ++) if(evs[i] == evs[i])this->evs[i] = evs[i];
}

void DiscountedCfrTrainable::updateRegrets(const vector<float>& regrets, int iteration_number, const vector<float>& reach_probs) {

#ifdef DEBUG
    if(regrets.size() != this->action_number * this->card_number) throw runtime_error("length not match");
#endif

    auto alpha_coef = pow(iteration_number, this->alpha);
    alpha_coef = alpha_coef / (1 + alpha_coef);

    fill(r_plus_sum.begin(),r_plus_sum.end(),0);
    for (int action_id = 0;action_id < action_number;action_id ++) {
        for(int private_id = 0;private_id < this->card_number;private_id ++){
            int index = action_id * this->card_number + private_id;
            float one_reg = regrets[index];

            this->r_plus[index] = one_reg + this->r_plus[index];
            if(this->r_plus[index] > 0){
                this->r_plus[index] *= alpha_coef;
            }else{
                this->r_plus[index] *= beta;
            }

            this->r_plus_sum[private_id] += max(float(0.0),this->r_plus[index]);
        }
    }
    vector<float> current_strategy = this->getcurrentStrategyNoCache();
    float strategy_coef = pow(((float)iteration_number / (iteration_number + 1)),gamma);
    for (int action_id = 0;action_id < action_number;action_id ++) {
        for(int private_id = 0;private_id < this->card_number;private_id ++) {
            int index = action_id * this->card_number + private_id;
            this->cum_r_plus[index] *= this->theta;
            this->cum_r_plus[index] += current_strategy[index] * strategy_coef;
        }
    }
}

void DiscountedCfrTrainable::dump_strategy(std::ostream& stream, bool with_state, const vector<vector<int>>& exchange_color_list, const shared_ptr<ActionNode>& node) {
    if (with_state) {
        stream << "{}";
        return;
    }

    vector<float> transformed_strategy = this->getAverageStrategy();

    if (!exchange_color_list.empty()) {
        const vector<PrivateCards>& range = *this->privateCards;
        unordered_map<int, int> hand_hash_to_index;
        for(size_t i = 0; i < range.size(); ++i) {
            hand_hash_to_index[range[i].hashCode()] = i;
        }

        for (const auto& one_exchange : exchange_color_list) {
            int rank1 = one_exchange[0];
            int rank2 = one_exchange[1];
            if (rank1 == rank2) continue;

            vector<bool> swapped(range.size(), false);
            for(std::size_t i = 0; i < range.size(); i++){
                if (swapped[i]) continue;

                const PrivateCards& pc_i = range[i];
                PrivateCards pc_j = pc_i.exchange_color(rank1, rank2);
                auto it = hand_hash_to_index.find(pc_j.hashCode());

                if (it != hand_hash_to_index.end()) {
                    size_t j = it->second;
                    if (i < j) {
                        for (int action_id = 0; action_id < this->action_number; ++action_id) {
                            size_t index_i = action_id * this->card_number + i;
                            size_t index_j = action_id * this->card_number + j;
                            std::swap(transformed_strategy[index_i], transformed_strategy[index_j]);
                        }
                        swapped[i] = true;
                        swapped[j] = true;
                    }
                }
            }
        }
    }

    stream << "{\"actions\":[";
    const auto& game_actions = action_node.getActions();
    for (size_t i = 0; i < game_actions.size(); ++i) {
        stream << "\"" << game_actions[i].toString() << "\"";
        if (i < game_actions.size() - 1) stream << ",";
    }
    stream << "],\"strategy\":{";

    for(std::size_t i = 0; i < this->privateCards->size(); i++) {
        const PrivateCards& one_private_card = (*this->privateCards)[i];
        stream << "\"" << one_private_card.toString() << "\":[";

        for(int j = 0; j < this->action_number; j++) {
            std::size_t strategy_index = j * this->card_number + i;
            stream << transformed_strategy[strategy_index];
            if (j < this->action_number - 1) stream << ",";
        }
        stream << "]";
        if (i < this->privateCards->size() - 1) stream << ",";
    }
    stream << "}}";
}


json DiscountedCfrTrainable::dump_strategy(bool with_state) {
    std::stringstream ss;
    dump_strategy(ss, with_state, {}, std::make_shared<ActionNode>(action_node));
    return json::parse(ss.str());
}

json DiscountedCfrTrainable::dump_evs() {
    json evs;
    const vector<float>& average_evs = this->evs;
    vector<GameActions>& game_actions = action_node.getActions();
    vector<string> actions_str;
    for(GameActions& one_action:game_actions) {
        actions_str.push_back(
                one_action.toString()
        );
    }

    for(std::size_t i = 0;i < this->privateCards->size();i ++){
        PrivateCards& one_private_card = (*this->privateCards)[i];
        vector<float> one_evs(this->action_number);

        for(int j = 0;j < this->action_number;j ++){
            std::size_t evs_index = j * this->privateCards->size() + i;
            one_evs[j] = average_evs[evs_index];
        }
        evs[tfm::format("%s",one_private_card.toString())] = one_evs;
    }

    json retjson;
    retjson["actions"] = std::move(actions_str);
    retjson["evs"] = std::move(evs);
    return std::move(retjson);
}

Trainable::TrainableType DiscountedCfrTrainable::get_type() {
    return DISCOUNTED_CFR_TRAINABLE;
}
