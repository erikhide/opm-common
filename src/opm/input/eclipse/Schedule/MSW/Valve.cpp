/*
  Copyright 2019 Equinor.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <opm/input/eclipse/Schedule/MSW/Valve.hpp>
#include <opm/input/eclipse/Deck/DeckRecord.hpp>
#include <opm/input/eclipse/Deck/DeckKeyword.hpp>


namespace Opm {

    ValveUDAEval::ValveUDAEval(const SummaryState& summary_state_, const std::string& well_name_,
                         const size_t segment_number_) :
        summary_state(summary_state_),
        well_name(well_name_),
        segment_number(segment_number_)
        {}


    double ValveUDAEval::value(const UDAValue& value, const double udq_default) const {
        if (value.is<double>())
            return value.getSI();

        const std::string& string_var = value.get<std::string>();
        double output_value = udq_default;

        if (summary_state.has_segment_var(well_name, string_var, segment_number+1))
            output_value = summary_state.get_segment_var(well_name, string_var, segment_number+1);
        else if (summary_state.has(string_var))
            output_value = summary_state.get(string_var);

        return value.get_dim().convertRawToSi(output_value);
    }


    Valve::Valve()
        : Valve(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, ICDStatus::SHUT)
    {
    }

    Valve::Valve(double conFlowCoeff,
                 double conCrossA,
                 double conMaxCrossA,
                 double pipeAddLength,
                 double pipeDiam,
                 double pipeRough,
                 double pipeCrossA,
                 ICDStatus stat)
        : m_con_flow_coeff(conFlowCoeff)
        , m_con_cross_area(UDAValue(conCrossA))
        , m_con_cross_area_value(conCrossA)
        , m_con_max_cross_area(conMaxCrossA)
        , m_pipe_additional_length(pipeAddLength)
        , m_pipe_diameter(pipeDiam)
        , m_pipe_roughness(pipeRough)
        , m_pipe_cross_area(pipeCrossA)
        , m_status(stat)
    {
    }

    Valve::Valve(const DeckRecord& record, const double udq_default)
        : m_con_flow_coeff(record.getItem("CV").get<double>(0))
        , m_con_cross_area(record.getItem("AREA").get<UDAValue>(0))
        , m_con_cross_area_value(m_con_cross_area.is<double>() ? m_con_cross_area.getSI() : -1.0)
        , m_udq_default(udq_default)
    {
        // we initialize negative values for the values are defaulted
        const double value_for_default = -1.e100;

        // TODO: we assume that the value input for this keyword is always positive
        if (record.getItem("EXTRA_LENGTH").defaultApplied(0)) {
            m_pipe_additional_length = value_for_default;
        } else {
            m_pipe_additional_length = record.getItem("EXTRA_LENGTH").getSIDouble(0);
        }

        if (record.getItem("PIPE_D").defaultApplied(0)) {
            m_pipe_diameter = value_for_default;
        } else {
            m_pipe_diameter = record.getItem("PIPE_D").getSIDouble(0);
        }

        if (record.getItem("ROUGHNESS").defaultApplied(0)) {
            m_pipe_roughness = value_for_default;
        } else {
            m_pipe_roughness = record.getItem("ROUGHNESS").getSIDouble(0);
        }

        if (record.getItem("PIPE_A").defaultApplied(0)) {
            m_pipe_cross_area = value_for_default;
        } else {
            m_pipe_cross_area = record.getItem("PIPE_A").getSIDouble(0);
        }

        if (record.getItem("STATUS").getTrimmedString(0) == "OPEN") {
            m_status = ICDStatus::OPEN;
        } else {
            m_status = ICDStatus::SHUT;
            // TODO: should we check illegal input here
        }

        if (record.getItem("MAX_A").defaultApplied(0)) {
            m_con_max_cross_area = value_for_default;
        } else {
            m_con_max_cross_area = record.getItem("MAX_A").getSIDouble(0);
        }
    }

    Valve Valve::serializationTestObject()
    {
        Valve result;
        result.m_con_flow_coeff = 1.0;
        result.m_con_cross_area = UDAValue(2.0);
        result.m_con_cross_area_value = 2.0;
        result.m_con_max_cross_area = 3.0;
        result.m_pipe_additional_length = 4.0;
        result.m_pipe_diameter = 5.0;
        result.m_pipe_roughness = 6.0;
        result.m_pipe_cross_area = 7.0;
        result.m_status = ICDStatus::OPEN;

        return result;
    }

    std::map<std::string, std::vector<std::pair<int, Valve> > >
    Valve::fromWSEGVALV(const DeckKeyword& keyword, const double udq_default)
    {
        std::map<std::string, std::vector<std::pair<int, Valve> > > res;

        for (const DeckRecord &record : keyword) {
            const std::string well_name = record.getItem("WELL").getTrimmedString(0);

            const int segment_number = record.getItem("SEGMENT_NUMBER").get<int>(0);

            const Valve valve(record, udq_default);
            res[well_name].push_back(std::make_pair(segment_number, valve));
        }

        return res;
    }

    ICDStatus Valve::status() const {
        return m_status;
    }

    double Valve::conFlowCoefficient() const {
        return m_con_flow_coeff;
    }

    double Valve::conCrossArea(const std::optional<const ValveUDAEval>& uda_eval_optional) {
        m_con_cross_area_value = uda_eval_optional.has_value() ?
                                    uda_eval_optional.value().value(m_con_cross_area, m_udq_default) :
                                    m_con_cross_area.getSI();
        return m_con_cross_area_value;
    }

    double Valve::pipeAdditionalLength() const {
        return m_pipe_additional_length;
    }

    double Valve::pipeDiameter() const {
        return m_pipe_diameter;
    }

    double Valve::pipeRoughness() const {
        return m_pipe_roughness;
    }

    double Valve::pipeCrossArea() const {
        return m_pipe_cross_area;
    }

    double Valve::conMaxCrossArea() const {
        return m_con_max_cross_area;
    }

    void Valve::setPipeDiameter(const double dia) {
        m_pipe_diameter = dia;
    }

    void Valve::setPipeRoughness(const double rou) {
        m_pipe_roughness = rou;
    }

    void Valve::setPipeCrossArea(const double area) {
        m_pipe_cross_area = area;
    }

    void Valve::setConMaxCrossArea(const double area) {
        m_con_max_cross_area = area;
    }

    void Valve::setPipeAdditionalLength(const double length) {
        m_pipe_additional_length = length;
    }

    bool Valve::operator==(const Valve& data) const {
        return this->conFlowCoefficient() == data.conFlowCoefficient() &&
               this->m_con_cross_area == data.m_con_cross_area &&
               this->conCrossAreaValue() == data.conCrossAreaValue() &&
               this->conMaxCrossArea() == data.conMaxCrossArea() &&
               this->pipeAdditionalLength() == data.pipeAdditionalLength() &&
               this->pipeDiameter() == data.pipeDiameter() &&
               this->pipeRoughness() == data.pipeRoughness() &&
               this->pipeCrossArea() == data.pipeCrossArea() &&
               this->status() == data.status();
    }
}
