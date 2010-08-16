/* 
 *  Copyright (c) 2010 Daisuke Okanohara 
 * 
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *   1. Redistributions of source code must retain the above Copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above Copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the authors nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 */

#include <algorithm>
#include <queue>
#include <fstream>
#include <cassert>
#include <map>
#include <cmath>
#include <iostream>
#include "ux.hpp"

using namespace std;

namespace ux_tool{

struct RangeNode{
  RangeNode(size_t _left, size_t _right) :
    left(_left), right(_right) {}
  size_t left;
  size_t right;
};
  
UX::UX() : vtailux_(NULL), tailIDLen_(0), keyNum_(0), isReady_(false) {
} 
  
UX::~UX(){
  delete vtailux_;
}
  
void UX::build(vector<string>& wordList, const bool isTailUX){
  sort(wordList.begin(), wordList.end());
  wordList.erase(unique(wordList.begin(), wordList.end()), wordList.end());
  
  keyNum_ = wordList.size();
  
  queue<RangeNode> q;
  queue<RangeNode> nextQ;
  if (keyNum_ != 0){
    q.push(RangeNode(0, keyNum_));
  }

  BitVec terminalBV;
  BitVec tailBV;
  BitVec loudBV;
  loudBV.push_back(0); // super root
  loudBV.push_back(1);
  
  for (size_t depth = 0;;){
    if (q.empty()){
      swap(q, nextQ);
      ++depth;
      if (q.empty()) break;
    }
    RangeNode& rn = q.front();
    const size_t left  = rn.left;
    const size_t right = rn.right;
    q.pop();
    
    string& cur = wordList[left];
    if (left + 1 == right &&
	depth + 1 < cur.size()){ // tail candidate
      loudBV.push_back(1);
      terminalBV.push_back(1);
      tailBV.push_back(1);
      string tail;
      for (size_t i = depth; i < cur.size(); ++i){
	tail += cur[i];
      }
      vtails_.push_back(tail);
      continue;
    } else {
      tailBV.push_back(0);
    }
    
    assert(wordList.size() > left);
    size_t newLeft = left;
    if (depth == cur.size()){
      terminalBV.push_back(1);
      ++newLeft;
      if (newLeft == right){
	loudBV.push_back(1);
	continue;
      }
    } else {
      terminalBV.push_back(0);
    }
    
    size_t  prev  = newLeft;
    assert(wordList[prev].size() > depth);
    uint8_t prevC = (uint8_t)wordList[prev][depth];
    uint32_t degree = 0;
    for (size_t i = prev+1; ; ++i){
      if (i < right && 
	  prevC == (uint8_t)wordList[i][depth]){
	continue;
      }
      edges_.push_back(prevC);
      loudBV.push_back(0);
      degree++;
      nextQ.push(RangeNode(prev, i));
      if (i == right){
	break;
      }
      prev  = i;
      assert(wordList[prev].size() > depth);
      prevC = wordList[prev][depth];
      
    }
    loudBV.push_back(1);
  }
  
  loud_.build(loudBV);
  terminal_.build(terminalBV);
  tail_.build(tailBV);
  
  if (keyNum_ > 0){
    isReady_ = true;
  }
  
  if (isTailUX){
    buildTailUX();
  }
}
  
int UX::save(const char* fn) const {
  ofstream ofs(fn);
  if (!ofs){
    return FILE_OPEN_ERROR;
  }
  return save(ofs);
}

int UX::load(const char* fn){
  ifstream ifs(fn);
  if (!ifs){
    return FILE_OPEN_ERROR;
  }
  return load(ifs);
}
  
int UX::save(std::ostream& os) const {
  loud_.save(os);
  terminal_.save(os);
  tail_.save(os);
  tailIDs_.save(os);
  
  os.write((const char*)&keyNum_, sizeof(keyNum_));
  size_t edgesSize = edges_.size();
  os.write((const char*)&edgesSize, sizeof(edgesSize));
  os.write((const char*)&edges_[0], sizeof(edges_[0]) * edges_.size()); 
  
  int useUX = (vtailux_ != NULL);
  os.write((const char*)&useUX, sizeof(useUX));
  if (useUX){
    int err = 0;
    if ((err = vtailux_->save(os)) != 0){
      return err;
    }
  } else {
    size_t tailsNum  = vtails_.size();
    os.write((const char*)&tailsNum,  sizeof(tailsNum));
    for (size_t i = 0; i < vtails_.size(); ++i){
      size_t tailSize = vtails_[i].size();
      os.write((const char*)&tailSize,  sizeof(tailSize));
      os.write((const char*)&vtails_[i][0], sizeof(vtails_[i][0]) * vtails_[i].size());
    }
  }
  
  if (!os){
    return SAVE_ERROR;
  }
  return 0;
}

int UX::load(std::istream& is){
  loud_.load(is);
  terminal_.load(is);
  tail_.load(is);
  tailIDs_.load(is);
  
  is.read((char*)&keyNum_, sizeof(keyNum_));
  size_t edgesSize = 0;
  is.read((char*)&edgesSize, sizeof(edgesSize));
  edges_.resize(edgesSize);
  is.read((char*)&edges_[0], sizeof(edges_[0]) * edges_.size());
  
  int useUX = 0;
  is.read((char*)&useUX, sizeof(useUX));
  if (useUX){
    vtailux_ = new UX;
    int err = 0;
    if ((err = vtailux_->load(is)) != 0){
      return err;
    }
    size_t tailNum = vtailux_->getKeyNum();
    tailIDLen_ = log2(tailNum); 
    
  } else {
    size_t tailsNum  = 0;
    is.read((char*)&tailsNum,  sizeof(tailsNum));
    vtails_.resize(tailsNum);
    for (size_t i = 0; i < tailsNum; ++i){
      size_t tailSize = 0;
      is.read((char*)&tailSize, sizeof(tailSize));
      vtails_[i].resize(tailSize);
      is.read((char*)&vtails_[i][0], sizeof(vtails_[i][0]) * vtails_[i].size());
    }
  }
  
  if (!is){
    return LOAD_ERROR;
  }
  isReady_ = true;
  return 0;
}
  
id_t UX::prefixSearch(const char* str, const size_t len, size_t& retLen) const{
  vector<id_t> retIDs;
  traverse(str, len, retLen, retIDs, 0xFFFFFFFF);
  if (retIDs.size() == 0){
    return NOTFOUND;
  }
  return retIDs.back();
}
  
size_t UX::commonPrefixSearch(const char* str, const size_t len, vector<id_t>& retIDs,
			      const size_t limit) const {
  retIDs.clear();
  size_t lastLen = 0;
  traverse(str, len, lastLen, retIDs, limit);
  return retIDs.size();
}
  
size_t UX::predictiveSearch(const char* str, const size_t len, vector<id_t>& retIDs, 
			    const size_t limit) const{
  retIDs.clear();
  if (!isReady_) return 0;
  if (limit == 0) return 0;
  
  uint32_t lastPos   = 0;
  uint32_t lastZeros = 0;
  uint32_t pos       = 2;
  uint32_t zeros     = 2;
  for (size_t i = 0; i < len; ++i){
    lastPos   = pos;
    lastZeros = zeros;
    getChild((uint8_t)str[i], pos, zeros);
    if (pos == NOTFOUND){
      return 0;
    }
  }
  
  // search all descendant nodes from curPos
  enumerateAll(lastPos, lastZeros, retIDs, limit);
  return retIDs.size();
}

void UX::decode(const id_t id, string& ret) const{
  ret.clear();
  if (!isReady_) return;
  
  uint32_t nodeID = terminal_.select(id+1, 1);
  
  uint32_t pos    = loud_.select(nodeID+1, 1) + 1;
  uint32_t zeros  = pos - nodeID;
  for (;;) { 
    uint8_t c = 0;
    getParent(c, pos, zeros);
    if (pos == 0) break;
    ret += (char)c;
  }
  reverse(ret.begin(), ret.end());
  if (tail_.getBit(nodeID)){
    ret += getTail(tail_.rank(nodeID, 1) - 1);
  }
}
  
string UX::decode(const id_t id) const {
  std::string ret;
  decode(id, ret);
  return ret;
}
  
size_t UX::getKeyNum() const {
  return keyNum_;
}
  
std::string UX::what(const int error){
  switch(error) {
  case 0:
    return string("succeeded");
  case FILE_OPEN_ERROR: 
    return string("file open error");
  case FILE_WRITE_ERROR:
    return string("file write error");
  case FILE_READ_ERROR:
    return string("file read error");
  default:
    return string("unknown error");
  }
}

size_t UX::getAllocSize() const{
  size_t retSize = 0;
  if (vtailux_) {
    retSize += vtailux_->getAllocSize();
    retSize += tailIDs_.getAllocSize();
  } else {
    size_t tailLenSum = 0;
    for (size_t i = 0; i < vtails_.size(); ++i){
      tailLenSum += vtails_[i].size();
    }
    retSize += tailLenSum + tailLenSum / 8; // length bit vector
  }
  return retSize + loud_.getAllocSize() + terminal_.getAllocSize() + 
    tail_.getAllocSize() + edges_.size();
}
  
void UX::allocStat(size_t allocSize, ostream& os) const{
  if (vtailux_) {
    vtailux_->allocStat(allocSize, os);
    size_t size = tailIDs_.getAllocSize();
    os << "tailIDs:\t" << size << "\t" << (float)size / allocSize << endl;
  } else {
    size_t tailLenSum = 0;
    for (size_t i = 0; i < vtails_.size(); ++i){
      tailLenSum += vtails_[i].size();
    }
    os << "   tails:\t" << tailLenSum << "\t" << (float)tailLenSum / allocSize << endl;
    os << " tailLen:\t" << tailLenSum/8 << "\t" << (float)tailLenSum/8 / allocSize << endl;
  }
  os << "    loud:\t" << loud_.getAllocSize() << "\t" << (float)loud_.getAllocSize() / allocSize << endl;
  os << "terminal:\t" << terminal_.getAllocSize() << "\t" << (float)terminal_.getAllocSize() / allocSize << endl;
  os << "    tail:\t" << tail_.getAllocSize() << "\t" << (float)tail_.getAllocSize() / allocSize << endl;
  os << "    edge:\t" << edges_.size() << "\t" << (float)edges_.size() / allocSize << endl;
}
  
void UX::stat(ostream & os) const {
  size_t tailslen = 0;
  for (size_t i = 0; i < vtails_.size(); ++i){
    tailslen += vtails_[i].size();
  }
  
  os << "   keyNum\t" << keyNum_ << endl
     << "    loud:\t" << loud_.size()     << endl
     << "terminal:\t" << terminal_.size() << endl
     << "    edge:\t" << edges_.size()    << endl
     << " avgedge:\t" << (float)edges_.size() / keyNum_ << endl
     << "  vtails:\t" << tailslen << endl
     << " tailnum:\t" << vtails_.size() << endl
     << " avgtail:\t" << (float)tailslen / keyNum_ << endl
     << endl;
}

  
void UX::buildTailUX(){
  vector<string> origTails = vtails_;
  try {
    vtailux_ = new UX;
  } catch (bad_alloc){
    isReady_ = false;
    return;
  }
  for (size_t i = 0; i < vtails_.size(); ++i){
    reverse(vtails_[i].begin(), vtails_[i].end());
  }
  vtailux_->build(vtails_, false);
  tailIDLen_ = log2(vtailux_->getKeyNum());
  
  for (size_t i = 0; i < origTails.size(); ++i){
    size_t retLen = 0;
    reverse(origTails[i].begin(), origTails[i].end());
    id_t id = vtailux_->prefixSearch(origTails[i].c_str(), origTails[i].size(), retLen);
    assert(id != NOTFOUND);
    assert(retLen == origTails[i].size());
    tailIDs_.push_back_with_len(id, tailIDLen_);
  }
  vector<string>().swap(vtails_);
}

void UX::getChild(const uint8_t c, uint32_t& pos, uint32_t& zeros) const {
  for (;; ++pos, ++zeros){
    if (loud_.getBit(pos)){
      pos = NOTFOUND;
      return;
    }
    assert(zeros >= 2);
    assert(edges_.size() > zeros-2);
    if (edges_[zeros-2] == c){
      pos   = loud_.select(zeros, 1)+1;
      zeros = pos - zeros + 1;
      return;
    }
  }
}

bool UX::isLeaf(const uint32_t pos) const {
  return loud_.getBit(pos);
}
  
void UX::getParent(uint8_t& c, uint32_t& pos, uint32_t& zeros) const {
  zeros = pos - zeros + 1;
  pos   = loud_.select(zeros, 0);
  if (zeros < 2) return;
  assert(edges_.size() > zeros-2);
  c     = edges_[zeros-2];
}  
  

void UX::traverse(const char* str, const size_t len, 
		  size_t& lastLen, std::vector<id_t>& retIDs, const size_t limit) const{
  lastLen = 0;
  if (!isReady_) return;
  if (limit == 0) return;
  
  uint32_t pos   = 2;
  uint32_t zeros = 2;
  for (size_t depth = 0; pos != NOTFOUND; ++depth){
    uint32_t ones = pos - zeros;
    
    if (tail_.getBit(ones)){
      size_t retLen = 0;
      if (tailMatch(str, len, depth, tail_.rank(ones, 1)-1, retLen)){
	lastLen = depth + retLen;
	retIDs.push_back(terminal_.rank(ones, 1) - 1);
      }
      break;
    } else if (terminal_.getBit(ones)){
      lastLen = depth;
      retIDs.push_back(terminal_.rank(ones, 1)-1);
      if (retIDs.size() == limit) {
	break;
      }
    }
    if (depth == len) break;
    getChild((uint8_t)str[depth], pos, zeros);
  }
}
  

void UX::enumerateAll(const uint32_t pos, const uint32_t zeros, vector<id_t>& retIDs, const size_t limit) const{
  const uint32_t ones = pos - zeros;
  if (terminal_.getBit(ones)){
    retIDs.push_back(terminal_.rank(ones, 1) - 1);
  }
  
  for (uint32_t i = 0; loud_.getBit(pos + i) == 0 &&
	 retIDs.size() < limit; ++i){
    uint32_t nextPos = loud_.select(zeros + i, 1)+1;
    enumerateAll(nextPos, nextPos - zeros - i + 1,  retIDs, limit);
  }
}
  


bool UX::tailMatch(const char* str, const size_t len, const size_t depth,
		   const uint32_t tailID, size_t& retLen) const{
  string tail = getTail(tailID);
  if (tail.size() > len-depth) {
    return false;
  }
  
  for (size_t i = 0; i < tail.size(); ++i){
    if (str[i+depth] != tail[i]) {
      return false;
    }
  }
  retLen = tail.size();
  return true;
}
  
std::string UX::getTail(const uint32_t i) const{
  if (vtailux_) {
    string ret;
    vtailux_->decode(tailIDs_.getBits(tailIDLen_ * i, tailIDLen_), ret);
    reverse(ret.begin(), ret.end());
    return ret;
  } else {
    return vtails_[i];
  }
}

}
