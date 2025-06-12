#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint16_t gNbNum = 1;
    uint16_t ueNum = 2;
    double simTime = 10.0;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    NodeContainer gNbs;
    gNbs.Create(gNbNum);

    NodeContainer ues;
    ues.Create(ueNum);

    MobilityHelper mobilityGnb;
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gNbs);

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                                "Distance", DoubleValue(1.0));
    mobilityUe.Install(ues);

    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 0.0));
    mobilityGnb.SetPositionAllocator(posAlloc);

    NrPointToPointEpcHelper epcHelper;
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.001)));
    epcHelper.SetS1uLinkAttributes(p2ph);

    InternetStackHelper internet;
    internet.Install(gNbs);
    internet.Install(ues);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.0.0", "255.255.255.0");

    NetDeviceContainer ueDevices;
    NetDeviceContainer gNbDevices;

    for (uint32_t i = 0; i < gNbNum; ++i) {
        for (uint32_t j = 0; j < ueNum; ++j) {
            NetDeviceContainer container = p2ph.Install(gNbs.Get(i), ues.Get(j));
            gNbDevices.Add(container.Get(0));
            ueDevices.Add(container.Get(1));
        }
    }

    Ipv4InterfaceContainer ueInterfaces = ipv4.Assign(ueDevices);
    Ipv4InterfaceContainer gNbInterfaces = ipv4.Assign(gNbDevices);

    Ipv4Address serverIp = ueInterfaces.GetAddress(1, 0);
    uint16_t serverPort = 8080;

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(serverIp, serverPort));
    ApplicationContainer serverApp = sink.Install(ues.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(serverIp, serverPort));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(ues.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}