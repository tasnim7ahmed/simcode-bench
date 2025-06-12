#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Nr5GSimpleUdp");

int
main(int argc, char *argv[])
{
    // Log components
    LogComponentEnable("Nr5GSimpleUdp", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);

    // Simulation parameters
    double simTime = 10.0;
    double udpPacketSize = 1024;
    uint32_t totalPackets = 1000;
    double packetInterval = 0.01; // seconds

    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(totalPackets));
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(Seconds(packetInterval)));
    Config::SetDefault("ns3::UdpClient::PacketSize", UintegerValue(udpPacketSize));

    // Create nodes: one UE and one gNB
    NodeContainer ueNodes;
    NodeContainer gnbNodes;
    ueNodes.Create(1);
    gnbNodes.Create(1);

    // Setup Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);
    mobility.Install(gnbNodes);

    // Install NR stack
    NrHelper nrHelper;

    // Set error models
    nrHelper.SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(100)));
    nrHelper.SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));

    // Create the grid scenario
    nr::GridScenarioParameters params;
    params.m_row = 1;
    params.m_col = 1;
    params.m_xMin = -200;
    params.m_xMax = 200;
    params.m_yMin = -200;
    params.m_yMax = 200;
    params.m_z = 1.5;
    params.m_channelNum = 1;
    params.m_interSiteDistance = 10.0;
    params.m_isUplink = true;
    params.m_randomX2y = false;

    nrHelper.SetScenario(params);

    NetDeviceContainer enbNetDev = nrHelper.InstallGnbDevice(gnbNodes);
    NetDeviceContainer ueNetDev = nrHelper.InstallUeDevice(ueNodes);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(gnbNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");

    Ipv4InterfaceContainer ueIpIface;
    Ipv4InterfaceContainer gnbIpIface;

    ueIpIface = ipv4h.Assign(ueNetDev);
    gnbIpIface = ipv4h.Assign(enbNetDev);

    // Attach UEs to gNBs
    nrHelper.AttachToClosestEnb(ueNetDev, enbNetDev);

    // Setup applications
    uint16_t port = 9; // UDP port for server

    // Server (UE)
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // Client (gNB)
    UdpClientHelper client(ueIpIface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(totalPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    client.SetAttribute("PacketSize", UintegerValue(udpPacketSize));

    ApplicationContainer clientApps = client.Install(gnbNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}