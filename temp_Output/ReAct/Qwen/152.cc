#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BusTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    CsmaChannelHelper channelHelper;
    channelHelper.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
    channelHelper.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

    NetDeviceContainer devices = channelHelper.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(3), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("bus-topology-simulation.tr");
    devices.Get(0)->GetChannel()->AddTraceSourceCallback("MacRx", MakeBoundCallback(&CsmaChannel::AsciiTraceSink, &asciiTraceHelper, stream));

    devices.Get(0)->GetChannel()->TraceConnectWithoutContext("PcapSniffer", MakeCallback([](Ptr<const Packet> p) {
        static uint32_t packetCount = 0;
        packetCount++;
        NS_LOG_INFO("Captured packet " << packetCount << " on bus.");
    }));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}