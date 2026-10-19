namespace nominal_pitch {
constexpr double master_clock_hz=3579545;
struct Carrier {double hz=0;unsigned crossings=0;};
Carrier frequency(const std::vector<int>& samples) {
    Carrier result;double first=0,last=0;
    for(unsigned i=0;i+1<samples.size();++i)if(samples[i]<=0&&samples[i+1]>0) {
        double position=i-double(samples[i])/(samples[i+1]-samples[i]);
        if(!result.crossings)first=position;last=position;++result.crossings;
    }
    if(result.crossings>=3)result.hz=master_clock_hz*(result.crossings-1)/(64*(last-first));
    return result;
}
void tests() {
    Chip c;c.voice(31,0,0,0,0,15,0x4a);c.write(0x58,1);c.write(8,0x40);
    c.step(179000);c.begin();c.frames(16384);c.end();
    for(unsigned output=0;output<2;++output) {
        const auto& samples=output?c.right:c.left;auto carrier=frequency(samples);auto bounds=std::minmax_element(samples.begin(),samples.end());std::string label=output?"nominal RIGHT":"nominal LEFT";
        check(label+" carrier qualified",carrier.crossings>=3&&*bounds.first< -1000&&*bounds.second>1000,carrier.crossings);
        check(label+" documented MUL1 440Hz",carrier.hz>=439&&carrier.hz<=441,carrier.hz);
    }
    c.write(8,0);c.step(1790016);c.begin();c.frames(4096);c.end();
    check("nominal complete stereo quiet",std::all_of(c.left.begin(),c.left.end(),[](int v){return v==0;})&&std::all_of(c.right.begin(),c.right.end(),[](int v){return v==0;}));
}
}
