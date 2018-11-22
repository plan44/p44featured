//
//  Copyright (c) 2017-2018 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of pixelboardd.
//
//  pixelboardd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  pixelboardd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with pixelboardd. If not, see <http://www.gnu.org/licenses/>.
//

#include "lifeview.hpp"

using namespace p44;


// MARK: ===== LifeView


LifeView::LifeView() :
  generationInterval(777*MilliSecond),
  lastGeneration(Never),
  staticcount(0),
  maxStatic(23),
  minStatic(10),
  minPopulation(9)
{
}

LifeView::~LifeView()
{
}


void LifeView::clear()
{
  cells.clear();
  prepareCells();
}


bool LifeView::prepareCells()
{
  PixelCoord csz = getContentSize();
  int numCells = csz.x*csz.y;
  if (numCells<=0) return false;
  if (numCells!=cells.size()) {
    cells.clear();
    for (int i=0; i<numCells; ++i) {
      cells.push_back(0);
    }
    makeDirty();
  }
  return true;
}



void LifeView::setGenerationInterval(MLMicroSeconds aInterval)
{
  generationInterval = aInterval;
}


#define MIN_SPAWN_START 3


MLMicroSeconds LifeView::step(MLMicroSeconds aPriorityUntil)
{
  MLMicroSeconds now = MainLoop::now();
  MLMicroSeconds nextCall = inherited::step(aPriorityUntil);
  if (generationInterval!=Never && now>=lastGeneration+generationInterval) {
    lastGeneration = now;
    nextGeneration();
  }
  updateNextCall(nextCall, now+generationInterval);
  return nextCall;
}


void LifeView::nextGeneration()
{
  calculateGeneration();
  if (dynamics==0) {
    staticcount++;
    LOG(LOG_NOTICE, "No dynamics for %d cycles, population is %d", staticcount, population);
    if (staticcount>maxStatic || (population<minPopulation && staticcount>minStatic)) {
      revive();
    }
  }
  else {
    if (staticcount>0) {
      staticcount -= 2;
      if (staticcount<0) staticcount = 0;
      LOG(LOG_NOTICE, "Dynamics (%+d) in this cycle, population is %d, reduced staticount to %d", dynamics, population, staticcount);
    }
    else {
      LOG(LOG_INFO, "Dynamics (%+d) in this cycle, population is %d", dynamics, population);
    }
  }
  makeDirty();
}


void LifeView::revive()
{
  // shoot in some new cells
  createRandomCells((int)cells.size()/20+1,(int)cells.size()/6+1);
  makeDirty();
}



int LifeView::cellindex(int aX, int aY, bool aWrap)
{
  if (aX<0) {
    if (!aWrap) return (int)cells.size(); // out of range
    aX += content.dx;
  }
  else if (aX>=content.dx) {
    if (!aWrap) return (int)cells.size(); // out of range
    aX -= content.dx;
  }
  if (aY<0) {
    if (!aWrap) return (int)cells.size(); // out of range
    aY += content.dy;
  }
  else if (aY>=content.dx) {
    if (!aWrap) return (int)cells.size(); // out of range
    aY -= content.dy;
  }
  return aY*content.dx+aX;
}


void LifeView::calculateGeneration()
{
  if (!prepareCells()) return;
  dynamics = 0;
  population = 0;
  // cell age 0 : dead for longer
  // cell age 1 : killed in this cycle
  // cell age 2...n : living, with
  // - 2 = created out of void
  // - 3 = spawned
  // - 4..n aged
  // first age all living cells by 1, clean those that were killed in last cycle
  for (int i=0; i<cells.size(); ++i) {
    int age = cells[i];
    if (age==1) {
      cells[i] = 0;
    }
    else if (age==2) {
      cells[i] = 4; // skip 3
    }
    else if (age>2) {
      cells[i]++; // just age normally
    }
  }
  // apply rules
  for (int x=0; x<content.dx; ++x) {
    for (int y=0; y<content.dy; y++) {
      int ci = cellindex(x, y, false);
      // calculate number of neighbours
      int nn = 0;
      for (int dx=-1; dx<2; dx+=1) {
        for (int dy=-1; dy<2; dy+=1) {
          if (dx!=0 || dy!=0) {
            // one of the 8 neighbours
            int nci = cellindex(x+dx, y+dy, true);
            if (nci<cells.size() && (cells[nci]>3 || cells[nci]==1)) {
              // is a living neighbour (exclude newborns of this cycle, include those that will die at end of this cycle
              nn++;
            }
          }
        }
      }
      // rules for living cells:
      if (cells[ci]>0) {
        // - Any live cell with fewer than two live neighbours dies,
        //   as if caused by underpopulation.
        // - Any live cell with more than three live neighbours dies, as if by overpopulation.
        if (nn<2 || nn>3) {
          cells[ci] = 1; // will die at end of this cycle (but still alive for calculation)
          dynamics--; // a dying cell is a transition
        }
        else {
          population++; // lives on, counts for population
        }
        // - Any live cell with two or three live neighbours lives on to the next generation.
      }
      // rule for dead cells:
      else {
        // - Any dead cell with exactly three live neighbours becomes a live cell, as if by reproduction.
        if (nn==3) {
          cells[ci] = 3; // spawned
          dynamics++; // a new cell is a transition
          population++; // now lives, counts for population
        }
      }
    }
  }
}


void LifeView::createRandomCells(int aMinCells, int aMaxCells)
{
  if (!prepareCells()) return;
  int numcells = aMinCells + rand() % (aMaxCells-aMinCells+1);
  while (numcells-- > 0) {
    int ci = rand() % cells.size();
    cells[ci] = 2; // created out of void
  }
  makeDirty();
}


typedef struct {
  int x;
  int y;
} PixPos;

typedef struct {
  int numpix;
  const PixPos *pixels;
} PixPattern;


static const PixPos blinker[] = { {0,-1}, {0,0}, {0,1} };
static const PixPos toad[] = { {1,0}, {2,0}, {3,0}, {0,1}, {1,1}, {2,1} };
static const PixPos beacon[] = { {0,0}, {1,0}, {0,1}, {3,2}, {2,3}, {3,3} };
static const PixPos pentadecathlon[] = {
  {-1,-5}, {0,-5}, {1,-5},
  {0,-4},
  {0,-3},
  {-1,-2}, {0,-2}, {1,-2},
  {-1,0}, {0,0}, {1,0},
  {-1,1}, {0,1}, {1,1},
  {-1,3}, {0,3}, {1,3},
  {0,4},
  {0,5},
  {-1,6}, {0,6}, {1,6}
};
static const PixPos rpentomino[] = { {0,-1}, {1,-1}, {-1,0}, {0,0}, {1,1} };
static const PixPos diehard[] = { {-3,0}, {-2,0}, {-2,1}, {2,1}, {3,-1}, {3,1}, {4,1} };
static const PixPos acorn[] = { {-3,1}, {-2,-1}, {-2,1}, {0,0}, {1,1}, {2,1}, {3,1} };
static const PixPos glider[] = { {0,-1}, {1,0}, {-1,1}, {0,1}, {1,1} };

#define PAT(x) { sizeof(x)/sizeof(PixPos), x }


static const PixPattern patterns[] = {
  PAT(blinker),
  PAT(toad),
  PAT(beacon),
  PAT(pentadecathlon),
  PAT(rpentomino),
  PAT(diehard),
  PAT(acorn),
  PAT(glider)
};
#define NUMPATTERNS (sizeof(patterns)/sizeof(PixPattern))


void LifeView::placePattern(uint16_t aPatternNo, bool aWrap, int aCenterX, int aCenterY, int aOrientation)
{
  if (!prepareCells()) return;
  if (aPatternNo>=NUMPATTERNS) return;
  if (aCenterX<0) aCenterX = rand() % content.dx;
  if (aCenterY<0) aCenterY = rand() % content.dy;
  if (aOrientation<0) aOrientation = rand() % 4;
  for (int i=0; i<patterns[aPatternNo].numpix; i++) {
    int x,y;
    PixPos px = patterns[aPatternNo].pixels[i];
    switch (aOrientation) {
      default: x=aCenterX+px.x; y=aCenterY+px.y; break;
      case 1: x=aCenterX+px.y; y=aCenterY-px.x; break;
      case 2: x=aCenterX-px.x; y=aCenterY-px.y; break;
      case 3: x=aCenterX-px.y; y=aCenterY+px.x; break;
    }
    int ci = cellindex(x, y, aWrap);
    cells[ci] = 2; // created out of void
  }
}



PixelColor LifeView::contentColorAt(PixelCoord aPt)
{
  PixelColor pix = transparent;
  // simplest colorisation: from yellow (young) to red
  int ci = cellindex(aPt.x, aPt.y, false);
  if (ci>=cells.size()) return pix; // out of range
  pix = backgroundColor;
  int age = cells[ci];
  if (age<2) return pix; // dead, background
  else if (age==2) {
    // artificially created
    pix.b = 255;
    pix.r = 200;
    pix.g = 220;
  }
  else if (age==3) {
    // grown
    pix.b = 0;
    pix.r = 128;
    pix.g = 255;
  }
  else {
    age -= 3;
    if (age<8) {
      pix.r = 255;
      pix.g = (7-age)*32; // remove green over first 8 cycles
    }
    else if (age<24) {
      pix.b = (age-8)*12; // add blue over next 16 cycles
      pix.r = 128+(23-age)*8; // ...and remove half of red already
    }
    else if (age<56) {
      pix.b = 192;
      pix.r = (55-age)*4; // remove rest of red over next 32 cycles
    }
    else {
      pix.b = 192; // stone age
    }
  }
  pix.a = 255; // opaque
  return pix;
}


/*
bool LifeView::handleKey(int aSide, KeyCodes aNewPressedKeys, KeyCodes aCurrentPressed)
{
  if (aNewPressedKeys) {
    if (aSide==0) {
      if (aNewPressedKeys & keycode_left)
        clear();
      else if (aNewPressedKeys & keycode_middleleft)
        createRandomCells(3,42);
      else if (aNewPressedKeys & keycode_middleright)
        postInfo("quit");
      else if (aNewPressedKeys & keycode_right)
        placePattern(5); // diehard
    }
    else {
      if (aNewPressedKeys & keycode_left)
        placePattern(2); // beacon
      else if (aNewPressedKeys & keycode_middleleft)
        placePattern(3, false, 5, 9, 0); // pentadecathlon centered
      else if (aNewPressedKeys & keycode_middleright)
        placePattern(7); // glider
      else if (aNewPressedKeys & keycode_right)
        placePattern(6); // acorn
    }
    timeNext();
    makeDirty();
    // reset count of static cycles / autospray trigger
    staticcount = 0;
  }
  return true; // fully handled
}
*/

#if ENABLE_VIEWCONFIG

// MARK: ===== view configuration

ErrorPtr LifeView::configureView(JsonObjectPtr aViewConfig)
{
  JsonObjectPtr o;
  ErrorPtr err = inherited::configureView(aViewConfig);
  if (Error::isOK(err)) {
    if (aViewConfig->get("generationinterval", o)) {
      generationInterval  = o->doubleValue()*MilliSecond;
    }
    if (aViewConfig->get("maxstatic", o)) {
      maxStatic  = o->doubleValue()*MilliSecond;
    }
    if (aViewConfig->get("minstatic", o)) {
      minStatic  = o->doubleValue()*MilliSecond;
    }
    if (aViewConfig->get("minpopulation", o)) {
      minPopulation  = o->doubleValue()*MilliSecond;
    }
    if (aViewConfig->get("addrandom", o)) {
      int c = o->int32Value();
      createRandomCells(c/3, c);
    }
    if (aViewConfig->get("addpattern", o)) {
      int p = o->int32Value();
      placePattern(p);
    }
  }
  return err;
}


#endif // ENABLE_VIEWCONFIG
