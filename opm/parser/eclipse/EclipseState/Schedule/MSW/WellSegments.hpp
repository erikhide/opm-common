/*
  Copyright 2015 SINTEF ICT, Applied Mathematics.

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

#ifndef SEGMENTSET_HPP_HEADER_INCLUDED
#define SEGMENTSET_HPP_HEADER_INCLUDED

#include <vector>
#include <map>

#include <opm/parser/eclipse/EclipseState/Schedule/MSW/Segment.hpp>

namespace Opm {
    class SpiralICD;
    class Valve;
}

namespace Opm {

    class DeckKeyword;

    class WellSegments {
    public:
        enum class LengthDepth{
            INC = 0,
            ABS = 1
        };
        static const std::string LengthDepthToString(LengthDepth enumValue);
        static LengthDepth LengthDepthFromString(const std::string& stringValue);


        enum class CompPressureDrop {
            HFA = 0,
            HF_ = 1,
            H__ = 2
        };
        static const std::string CompPressureDropToString(CompPressureDrop enumValue);
        static CompPressureDrop CompPressureDropFromString(const std::string& stringValue);


        enum class MultiPhaseModel {
            HO = 0,
            DF = 1
        };
        static const std::string MultiPhaseModelToString(MultiPhaseModel enumValue);
        static MultiPhaseModel MultiPhaseModelFromString(const std::string& stringValue);


        WellSegments() = default;
        WellSegments(CompPressureDrop compDrop,
                     const std::vector<Segment>& segments);
        explicit WellSegments(const DeckKeyword& keyword);

        std::size_t size() const;
        double depthTopSegment() const;
        double lengthTopSegment() const;
        double volumeTopSegment() const;

        CompPressureDrop compPressureDrop() const;

        // mapping the segment number to the index in the vector of segments
        int segmentNumberToIndex(const int segment_number) const;



        const Segment& getFromSegmentNumber(const int segment_number) const;

        const Segment& operator[](size_t idx) const;
        void orderSegments();

        bool operator==( const WellSegments& ) const;
        bool operator!=( const WellSegments& ) const;

        double segmentLength(const int segment_number) const;

        // it returns true if there is no error encountered during the update
        bool updateWSEGSICD(const std::vector<std::pair<int, SpiralICD> >& sicd_pairs);

        bool updateWSEGVALV(const std::vector<std::pair<int, Valve> >& valve_pairs);
        const std::vector<Segment>::const_iterator begin() const;
        const std::vector<Segment>::const_iterator end() const;

        template<class Serializer>
        void serializeOp(Serializer& serializer)
        {
            serializer(m_comp_pressure_drop);
            serializer.vector(m_segments);
            serializer(segment_number_to_index);
        }

    private:
        void processABS();
        void processINC(double depth_top, double length_top);
        void process(LengthDepth length_depth, double depth_top, double length_top);
        void addSegment(const Segment& new_segment);
        void loadWELSEGS( const DeckKeyword& welsegsKeyword);
        const Segment& topSegment() const;

        // components of the pressure drop to be included
        CompPressureDrop m_comp_pressure_drop;
        // There are X and Y cooridnate of the nodal point of the top segment
        // Since they are not used for simulations and we are not supporting plotting,
        // we are not handling them at the moment.
        // There are other three properties for segment related to thermal conduction,
        // while they are not supported by the keyword at the moment.

        std::vector< Segment > m_segments;
        // the mapping from the segment number to the
        // storage index in the vector
        std::map<int, int> segment_number_to_index;
    };
}

#endif
