//
// Created by clo on 13/09/2019.
//

#ifndef SPEECH_ANALYSIS_PITCH_H
#define SPEECH_ANALYSIS_PITCH_H

#include <Eigen/Core>

namespace Pitch {

    struct Estimation {
        double pitch;
        bool isVoiced;
    };

    void estimate_AMDF(const Eigen::ArrayXd & x, double fs, Pitch::Estimation & result, double F0min, double F0max, double ratio, double sensitivity);

}

#endif //SPEECH_ANALYSIS_PITCH_H