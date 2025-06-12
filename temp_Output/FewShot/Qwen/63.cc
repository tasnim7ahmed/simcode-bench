#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool tracing = false;
    bool useNanoSec = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("useNanoSec", "Use nanoseconds resolution in pcap trace", useNanoSec);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));

    PacketSinkHelper packetSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Send forever until stopped
    ApplicationContainer clientApp = bulkSend.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    if (tracing) {
        pointToPoint.EnablePcap("tcp-pcap-nanosec-example.pcap", devices.Get(1), useNanoSec, true);
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}