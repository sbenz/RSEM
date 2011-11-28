#include <cstdio>
#include <string>
#include <vector>
#include <ostream>

struct Wiggle {
    std::string name;
    std::vector<float> read_depth;
};

class WiggleProcessor {
public:
    virtual ~WiggleProcessor() {}
    virtual void process(const Wiggle& wiggle) = 0;
};

class UCSCWiggleTrackWriter : public WiggleProcessor {
public:
    UCSCWiggleTrackWriter(const std::string& output_filename,
                          const std::string& track_name);
        
    ~UCSCWiggleTrackWriter();

    void process(const Wiggle& wiggle);

private:
    FILE *fo;
};

class ReadDepthWriter : public WiggleProcessor {
public:
    ReadDepthWriter(std::ostream& stream);

    void process(const Wiggle& wiggle);

private:
    std::ostream& stream_;
};

void build_wiggles(const std::string& bam_filename,
                   WiggleProcessor& processor);