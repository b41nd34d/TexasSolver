// Half float version created by Martin Ostermann on 2022-5-12,
// based DiscountableCfrTrainable.h from Xuefeng Huang on 2020/1/31.

#include "include/trainable/DiscountedCfrTrainableHF.h"
#include <sstream>

//#define DEBUG;

DiscountedCfrTrainableHF::DiscountedCfrTrainableHF(vector<PrivateCards> *privateCards,
                                               ActionNode &actionNode) : action_node(actionNode) {
    this->privateCards = privateCards;
    this->action_number = action_node.getChildrens().size();
    this->card_number = privateCards->size();
    this->evs = vector<EvsStorage>(this->action_number * this->card_number, (EvsStorage) 0.0);
    this->r_plus = vector<RplusStorage>(this->action_number * this->card_number, (RplusStorage) 0.0);
    this->cum_r_plus = vector<CumRplusStorage>(this->action_number * this->card_number, (CumRplusStorage) 0.0);
}

bool DiscountedCfrTrainableHF::isAllZeros(const vector<float>& input_array) {
    for(float i:input_array){
        if (i != 0)return false;
    }
    return true;
}

const vector<float> DiscountedCfrTrainableHF::getAverageStrategy() {
    vector<float> average_strategy;
    average_strategy = vector<float>(this->action_number * this->card_number);
    for (int private_id = 0; private_id < this->card_number; private_id++) {
        float r_plus_sum = 0;
        for (int action_id = 0; action_id < action_number; action_id++) {
            int index = action_id * this->card_number + private_id;
            average_strategy[index] = this->cum_r_plus[index];
            r_plus_sum += average_strategy[index];
        }

        for (int action_id = 0; action_id < action_number; action_id++) {
            int index = action_id * this->card_number + private_id;
            if(r_plus_sum) {
                // we stored this->cum_r_plus[index] in average_strategy[index] above
                // this is to avoid converting from half float twice
                average_strategy[index] = average_strategy[index] / r_plus_sum;
            }else{
                average_strategy[index] = 1.0 / this->action_number;
            }
        }
    }
    return average_strategy;
}

const vector<float> DiscountedCfrTrainableHF::getcurrentStrategy() {
    return this->getcurrentStrategyNoCache();
}

void DiscountedCfrTrainableHF::copyStrategy(shared_ptr<Trainable> other_trainable){
    shared_ptr<DiscountedCfrTrainableHF> trainable = dynamic_pointer_cast<DiscountedCfrTrainableHF>(other_trainable);
    this->r_plus.assign(trainable->r_plus.begin(),trainable->r_plus.end());
    this->cum_r_plus.assign(trainable->cum_r_plus.begin(),trainable->cum_r_plus.end());
}

const vector<float> DiscountedCfrTrainableHF::getcurrentStrategyNoCache() {
    vector<float> current_strategy;
    current_strategy = vector<float>(this->action_number * this->card_number);
    // calculate r_plus_sum on the fly, store r_plus as floats locally
    vector<float> r_plus_sum = vector<float>(this->r_plus.size());
    vector<float> r_plus = vector<float>(this->r_plus.size());
    fill(r_plus_sum.begin(),r_plus_sum.end(),0);
    for (int action_id = 0;action_id < action_number;action_id ++) {
        for(int private_id = 0;private_id < this->card_number;private_id ++){
            int index = action_id * this->card_number + private_id;
            float this_r_plus_of_index = this->r_plus[index];
            r_plus_sum[private_id] += max(float(0.0),this_r_plus_of_index);
            r_plus[index] = this_r_plus_of_index;
        }
    }

    for (int action_id = 0; action_id < action_number; action_id++) {
        for (int private_id = 0; private_id < this->card_number; private_id++) {
            int index = action_id * this->card_number + private_id;
            if(r_plus_sum[private_id] != 0) {
                current_strategy[index] = max(float(0.0),r_plus[index]) / r_plus_sum[private_id];
            }else{
                current_strategy[index] = 1.0 / (this->action_number);
            }
#ifdef DEBUG
            if(this->r_plus[index] != this->r_plus[index]) throw runtime_error("nan found");
#endif
        }
    }
    return current_strategy;
}

void DiscountedCfrTrainableHF::setEv(const vector<float>& evs){
    if(evs.size() != this->evs.size()) throw runtime_error("size mismatch in discountcfrtrainable setEV");
    for(std::size_t i = 0;i < evs.size();i ++) if(evs[i] == evs[i])this->evs[i] = evs[i];
}

void DiscountedCfrTrainableHF::updateRegrets(const vector<float>& regrets, int iteration_number, const vector<float>& reach_probs) {

#ifdef DEBUG
    if(regrets.size() != this->action_number * this->card_number) throw runtime_error("length not match");
#endif

    auto alpha_coef = pow(iteration_number, this->alpha);
    alpha_coef = alpha_coef / (1 + alpha_coef);

    vector<float> r_plus_sum = vector<float>(this->r_plus.size());
    vector<float> r_plus = vector<float>(this->r_plus.size());
    fill(r_plus_sum.begin(),r_plus_sum.end(),0);
    for (int action_id = 0;action_id < action_number;action_id ++) {
        for(int private_id = 0;private_id < this->card_number;private_id ++){
            int index = action_id * this->card_number + private_id;
            float one_reg = regrets[index];

            // 更新 R+
            float this_r_plus_of_index = this->r_plus[index];
            this_r_plus_of_index = one_reg + this_r_plus_of_index;
            if(this_r_plus_of_index > 0){
                this_r_plus_of_index *= alpha_coef;
            }else{
                this_r_plus_of_index *= beta;
            }
            r_plus_sum[private_id] += max(float(0.0),this_r_plus_of_index);
            this->r_plus[index] = this_r_plus_of_index;
            r_plus[index] = this_r_plus_of_index;
        }
    }

    // inline replacement with less conversion induced precision loss and overhead of
    // vector<float> current_strategy = this->getcurrentStrategyNoCache();
    vector<float> current_strategy = vector<float>(this->action_number * this->card_number);
    for (int action_id = 0; action_id < action_number; action_id++) {
        for (int private_id = 0; private_id < this->card_number; private_id++) {
            int index = action_id * this->card_number + private_id;
            if(r_plus_sum[private_id] != 0) {
                current_strategy[index] = max(float(0.0), r_plus[index]) / r_plus_sum[private_id];
            }else{
                current_strategy[index] = 1.0 / (this->action_number);
            }
#ifdef DEBUG
            if(this->r_plus[index] != this->r_plus[index]) throw runtime_error("nan found");
#endif
        }
    }
    // end of inline replacement

    float strategy_coef = pow(((float)iteration_number / (iteration_number + 1)),gamma);
    for (int action_id = 0;action_id < action_number;action_id ++) {
        for(int private_id = 0;private_id < this->card_number;private_id ++) {
            int index = action_id * this->card_number + private_id;
            this->cum_r_plus[index] = this->cum_r_plus[index] * this->theta +
                current_strategy[index] * strategy_coef;// * reach_probs[private_id];
        }
    }
}

void DiscountedCfrTrainableHF::dump_strategy(std::ostream& stream, bool with_state, const vector<vector<int>>& exchange_color_list, const shared_ptr<ActionNode>& node) {
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
    const auto& game_actions = node->getActions();
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

json DiscountedCfrTrainableHF::dump_strategy(bool with_state) {
    std::stringstream ss;
    dump_strategy(ss, with_state, {}, std::make_shared<ActionNode>(action_node));
    return json::parse(ss.str());
}

json DiscountedCfrTrainableHF::dump_evs() {
    json evs;
    const vector<EvsStorage>& average_evs = this->evs;
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
            int evs_index = j * this->privateCards->size() + i;
            one_evs[j] = average_evs[evs_index];
        }
        evs[tfm::format("%s",one_private_card.toString())] = one_evs;
    }

    json retjson;
    retjson["actions"] = std::move(actions_str);
    retjson["evs"] = std::move(evs);
    return std::move(retjson);
}

Trainable::TrainableType DiscountedCfrTrainableHF::get_type() {
    return DISCOUNTED_CFR_TRAINABLE;
}
