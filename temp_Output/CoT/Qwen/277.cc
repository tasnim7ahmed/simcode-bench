#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint16_t numGnbs = 1;
    uint16_t numUes = 1;
    double simTime = 10.0;
    double interPacketInterval = 1.0;
    uint32_t packetSize = 512;
    uint32_t numPackets = 5;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    NodeContainer gnbNodes;
    gnbNodes.Create(numGnbs);

    NodeContainer ueNodes;
    ueNodes.Create(numUes);

    MobilityHelper mobilityGnb;
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=100.0]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                                "Distance", DoubleValue(5.0));
    mobilityUe.Install(ueNodes);

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetGnbDeviceAttribute("Numerology", UintegerValue(1));
    nrHelper->SetUeDeviceAttribute("Numerology", UintegerValue(1));

    NetDeviceContainer gnbDevs;
    gnbDevs = nrHelper->InstallGnbDevice(gnbNodes, ueNodes);

    NetDeviceContainer ueDevs;
    ueDevs = nrHelper->InstallUeDevice(ueNodes, gnbDevs);

    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4GnbAddr;
    ipv4GnbAddr.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer gnbIpv4 = ipv4GnbAddr.Assign(gnbDevs);

    Ipv4AddressHelper ipv4UeAddr;
    ipv4UeAddr.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpv4 = ipv4UeAddr.Assign(ueDevs);

    uint16_t dlPort = 12345;

    PacketSinkHelper dlPacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    ApplicationContainer sinkApps = dlPacketSinkHelper.Install(gnbNodes.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime));

    OnOffHelper onOffTcpClient("ns3::TcpSocketFactory", Address());
    onOffTcpClient.SetAttribute("Remote", AddressValue(InetSocketAddress(gnbIpv4.GetAddress(0), dlPort)));
    onOffTcpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffTcpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffTcpClient.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onOffTcpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    onOffTcpClient.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numPackets; ++i) {
        AddressValue remoteAddress(InetSocketAddress(gnbIpv4.GetAddress(0), dlPort));
        onOffTcpClient.SetAttribute("Remote", remoteAddress);
        ApplicationContainer app = onOffTcpClient.Install(ueNodes.Get(0));
        app.Start(Seconds(i * interPacketInterval));
        app.Stop(Seconds(i * interPacketInterval + 0.5));
        clientApps.Add(app);
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}