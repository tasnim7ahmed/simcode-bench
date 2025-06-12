#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FiveGSimulation");

int main(int argc, char *argv[])
{
    uint16_t numEnbs = 1;
    uint16_t numUes = 2;
    double simTime = 10.0;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    Config::SetDefault("ns3::RvBatteryModel::InitialEnergy", DoubleValue(1.0e6));

    NodeContainer enbNodes;
    enbNodes.Create(numEnbs);

    NodeContainer ueNodes;
    ueNodes.Create(numUes);

    MobilityHelper mobilityEnb;
    mobilityEnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(50.0),
                                     "DeltaY", DoubleValue(50.0),
                                     "GridWidth", UintegerValue(3),
                                     "LayoutType", StringValue("RowFirst"));
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobilityUe.Install(ueNodes);

    NrPointToPointEpcHelper epcHelper;
    Ptr<Node> pgw = epcHelper.GetPgwNode();

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    NetDeviceContainer enbDev;
    enbDev = epcHelper.InstallEnbDevice(enbNodes.Get(0));
    NetDeviceContainer ueDevs;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        NetDeviceContainer ueDev = epcHelper.InstallUeDevice(ueNodes.Get(i));
        ueDevs.Add(ueDev);
    }

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIfaces;
    for (auto it = ueDevs.Begin(); it != ueDevs.End(); ++it)
    {
        Ipv4InterfaceContainer iface = ipv4h.Assign(*it);
        ueIpIfaces.Add(iface);
    }

    uint16_t dlPort = 8080;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps;
    sinkApps.Add(packetSinkHelper.Install(ueNodes.Get(1)));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    InetSocketAddress rmtAddr(ueIpIfaces.GetAddress(1), dlPort);
    clientHelper.SetAttribute("Remote", AddressValue(rmtAddr));
    ApplicationContainer clientApp = clientHelper.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}