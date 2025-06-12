#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPcapNanosecExample");

int main(int argc, char *argv[]) {
    bool enablePcap = true;
    bool useNanoSec = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("pcap", "Enable pcap output", enablePcap);
    cmd.AddValue("useNanoSec", "Use nanoseconds resolution in pcap file", useNanoSec);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    NS_LOG_INFO("Creating channel.");
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    NS_LOG_INFO("Installing internet stack.");
    InternetStackHelper stack;
    stack.Install(nodes);

    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    NS_LOG_INFO("Creating applications.");
    uint16_t port = 9;

    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(0.0));
    sourceApps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    NS_LOG_INFO("Enabling global routing.");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    if (enablePcap) {
        PcapHelper pcapHelper;
        std::string filename = "tcp-pcap-nanosec-example.pcap";
        if (useNanoSec) {
            pcapHelper.EnablePcap(filename, devices, false, true);
        } else {
            pcapHelper.EnablePcap(filename, devices, false);
        }
    }

    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Done.");
    return 0;
}