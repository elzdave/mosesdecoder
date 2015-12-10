/*
 * Distortion.cpp
 *
 *  Created on: 28 Oct 2015
 *      Author: hieu
 */
#include <sstream>
#include "Distortion.h"
#include "../Search/Hypothesis.h"
#include "../Search/Manager.h"
#include "../legacy/Range.h"
#include "../legacy/Bitmap.h"

using namespace std;

namespace Moses2
{

struct DistortionState_traditional : public FFState {
  Range range;
  int first_gap;

  DistortionState_traditional()
  :range()
  {
	  // uninitialised
  }

  void Set(const Range& wr, int fg)
  {
	 range = wr;
	 first_gap = fg;
  }

  size_t hash() const {
    return range.GetEndPos();
  }
  virtual bool operator==(const FFState& other) const {
    const DistortionState_traditional& o =
      static_cast<const DistortionState_traditional&>(other);
    return range.GetEndPos() == o.range.GetEndPos();
  }

  virtual std::string ToString() const
  {
	  stringstream sb;
	  sb << first_gap << " " << range;
	  return sb.str();
  }

};


///////////////////////////////////////////////////////////////////////
Distortion::Distortion(size_t startInd, const std::string &line)
:StatefulFeatureFunction(startInd, line)
{
	ReadParameters();
}

Distortion::~Distortion() {
	// TODO Auto-generated destructor stub
}

FFState* Distortion::BlankState(const Manager &mgr, const PhraseImpl &input) const
{
  MemPool &pool = mgr.GetPool();
  return new (pool.Allocate<DistortionState_traditional>()) DistortionState_traditional();
}

void Distortion::EmptyHypothesisState(FFState &state, const Manager &mgr, const PhraseImpl &input) const
{
    DistortionState_traditional &stateCast = static_cast<DistortionState_traditional&>(state);

	  // fake previous translated phrase start and end
	  size_t start = NOT_FOUND;
	  size_t end = NOT_FOUND;
	  /*
	  if (input.m_frontSpanCoveredLength > 0) {
	    // can happen with --continue-partial-translation
	    start = 0;
	    end = input.m_frontSpanCoveredLength -1;
	  }
	  */

	    stateCast.range = Range(start, end);
	    stateCast.first_gap = NOT_FOUND;
}

void
Distortion::EvaluateInIsolation(const System &system,
		const Phrase &source, const TargetPhrase &targetPhrase,
		Scores &scores,
		Scores *estimatedScores) const
{
}

void Distortion::EvaluateWhenApplied(const Manager &mgr,
  const Hypothesis &hypo,
  const FFState &prevState,
  Scores &scores,
  FFState &state) const
{
	MemPool &pool = mgr.GetPool();

    const DistortionState_traditional &prev = static_cast<const DistortionState_traditional&>(prevState);
	SCORE distortionScore = CalculateDistortionScore(
            prev.range,
            hypo.GetRange(),
            prev.first_gap);
	//cerr << "distortionScore=" << distortionScore << endl;

	scores.PlusEquals(mgr.system, *this, distortionScore);

    DistortionState_traditional &stateCast = static_cast<DistortionState_traditional&>(state);
    stateCast.Set(hypo.GetRange(), hypo.GetBitmap().GetFirstGapPos());
}

SCORE Distortion::CalculateDistortionScore(const Range &prev, const Range &curr, const int FirstGap) const
{
  bool useEarlyDistortionCost = false;
  if(!useEarlyDistortionCost) {
    return - (SCORE) ComputeDistortionDistance(prev, curr);
  } else {
    /* Pay distortion score as soon as possible, from Moore and Quirk MT Summit 2007
       Definitions:
       S   : current source range
       S'  : last translated source phrase range
       S'' : longest fully-translated initial segment
    */

    int prefixEndPos = (int)FirstGap-1;
    if((int)FirstGap==-1)
      prefixEndPos = -1;

    // case1: S is adjacent to S'' => return 0
    if ((int) curr.GetStartPos() == prefixEndPos+1) {
      //IFVERBOSE(4) std::cerr<< "MQ07disto:case1" << std::endl;
      return 0;
    }

    // case2: S is to the left of S' => return 2(length(S))
    if ((int) curr.GetEndPos() < (int) prev.GetEndPos()) {
      //IFVERBOSE(4) std::cerr<< "MQ07disto:case2" << std::endl;
      return (float) -2*(int)curr.GetNumWordsCovered();
    }

    // case3: S' is a subsequence of S'' => return 2(nbWordBetween(S,S'')+length(S))
    if ((int) prev.GetEndPos() <= prefixEndPos) {
      //IFVERBOSE(4) std::cerr<< "MQ07disto:case3" << std::endl;
      int z = (int)curr.GetStartPos()-prefixEndPos - 1;
      return (float) -2*(z + (int)curr.GetNumWordsCovered());
    }

    // case4: otherwise => return 2(nbWordBetween(S,S')+length(S))
    //IFVERBOSE(4) std::cerr<< "MQ07disto:case4" << std::endl;
    return (float) -2*((int)curr.GetNumWordsBetween(prev) + (int)curr.GetNumWordsCovered());

  }
}

int Distortion::ComputeDistortionDistance(const Range& prev, const Range& current) const
{
  int dist = 0;
  if (prev.GetNumWordsCovered() == 0) {
    dist = current.GetStartPos();
  } else {
    dist = (int)prev.GetEndPos() - (int)current.GetStartPos() + 1 ;
  }
  return abs(dist);
}

}

