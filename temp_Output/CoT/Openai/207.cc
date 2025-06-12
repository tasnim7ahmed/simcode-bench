#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

uint64_t totalBytesReceived = 0;

void RxCallback(Ptr<const Packet> packet, const Address &address)
{
    totalBytesReceived += packet->GetSize();
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(20.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(0), port));

    // TCP Server (Sink) on node 0
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(11.0));

    // Attach throughput calculation callback
    Ptr<Application> app = sinkApp.Get(0);
    Ptr<PacketSink> pktSink = DynamicCast<PacketSink>(app);
    pktSink->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));

    // TCP Client (OnOff) on node 1
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(1));

    AnimationInterface anim("wifi-tcp-aodv.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 20.0);
    anim.SetConstantPosition(nodes.Get(1), 30.0, 20.0);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    double throughput = (totalBytesReceived * 8.0) / (8.0 * 1e6); // Mbps
    std::cout << "Total received bytes: " << totalBytesReceived << std::endl;
    std::cout << "Average throughput: " << throughput / 8.0 << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}