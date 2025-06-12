#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time
    double simTime = 10.0;
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    // Create nodes: one gNodeB and one UE
    NodeContainer gnbNodes;
    NodeContainer ueNodes;
    gnbNodes.Create(1);
    ueNodes.Create(1);

    // Setup Mobility for UE (Random Walk in 100x100 area)
    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                                "Distance", DoubleValue(10));
    mobilityUe.Install(ueNodes);

    // gNodeB is static
    MobilityHelper mobilityGnb;
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbNodes);

    // Install NR spectrum and propagation
    NrHelper nrHelper;
    nrHelper.SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(100)));
    nrHelper.SetPathlossModelType(TypeId::LookupByName("ns3::ThreeGppUmiStreetCanyonPropagationLossModel"));
    nrHelper.SetPathlossModelAttribute("ShadowingEnabled", BooleanValue(false));

    // Create the grid scenario
    nrHelper.Initialize();
    NetDeviceContainer enbDev = nrHelper.InstallGnbDevice(gnbNodes, nrHelper.CreateBand());
    NetDeviceContainer ueDev = nrHelper.InstallUeDevice(ueNodes, enbDev);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipAddrs;
    ipAddrs.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer gnbIpIface = ipAddrs.Assign(enbDev);
    ipAddrs.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = ipAddrs.Assign(ueDev);

    // TCP Server on gNodeB
    uint16_t tcpPort = 50000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(gnbNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // TCP Client on UE
    AddressValue remoteAddress(InetSocketAddress(gnbIpIface.GetAddress(0), tcpPort));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(512));
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", Address());
    bulkSend.SetAttribute("Remote", remoteAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(5 * 512)); // Send 5 packets of 512 bytes
    ApplicationContainer clientApp = bulkSend.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}