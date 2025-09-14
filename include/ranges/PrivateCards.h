//
// Created by Xuefeng Huang on 2020/1/31.
//

#ifndef TEXASSOLVER_PRIVATECARDS_H
#define TEXASSOLVER_PRIVATECARDS_H
#include "include/Card.h"

class PrivateCards {
public:
    int card1{};
    int card2{};
    float weight{};
    float relative_prob{};
    PrivateCards();
    PrivateCards(int card1, int card2, float weight);
    uint64_t toBoardLong() const;
    int hashCode() const;
    string toString() const;
    const vector<int> & get_hands() const;
    PrivateCards exchange_color(int rank1, int rank2) const {
        int new_card1 = this->card1;
        int new_card2 = this->card2;

        if (this->card1 % 4 == rank1) {
            new_card1 = this->card1 - rank1 + rank2;
        } else if (this->card1 % 4 == rank2) {
            new_card1 = this->card1 - rank2 + rank1;
        }

        if (this->card2 % 4 == rank1) {
            new_card2 = this->card2 - rank1 + rank2;
        } else if (this->card2 % 4 == rank2) {
            new_card2 = this->card2 - rank2 + rank1;
        }
        return PrivateCards(new_card1, new_card2, this->weight);
    }
private:
    vector<int> card_vec;
    int hash_code{};
    uint64_t board_long;
};


#endif //TEXASSOLVER_PRIVATECARDS_H
