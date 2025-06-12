#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrTcpRandomWalkUe");

int main(int argc, char *argv[])
{
    // Log components
    LogComponentEnable("NrTcpRandomWalkUe", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Simulation time
    double simTime = 10.0;
    Config::SetDefault("ns3::ConfigStore::Filename", StringValue("input-defaults.txt"));
    Config::SetDefault("ns3::ConfigStore::Mode", StringValue("Load"));

    // Create gNodeB and UE nodes
    NodeContainer gnbNodes;
    gnbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Setup mobility for the UE with Random Walk
    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                                "Distance", DoubleValue(10.0));
    mobilityUe.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                                    "X", DoubleValue(0.0),
                                    "Y", DoubleValue(0.0),
                                    "rho", DoubleValue(50.0));
    mobilityUe.Install(ueNodes);

    // Mobility for gNodeB (static)
    MobilityHelper mobilityGnb;
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbNodes);

    // Install NR devices
    NrPointToPointEpcHelper epcHelper;
    NrHelper nrHelper;

    // Set scheduler
    nrHelper.SetSchedulerTypeId("ns3::nr::NrMacSchedulerTdmaRR");

    // Carrier configuration
    BandwidthPartInfoPtrVector allBwps;

    // Create BWP for gNodeB and UEs
    auto bwp = std::make_shared<BandwidthPartInfo>();
    bwp->m_bwpId = 0;
    bwp->m_numerology.m_subcarrierSpacing = 30;
    bwp->m_numerology.m_cyclicPrefix = CyclicPrefix::NORMAL;
    bwp->m_channelBandwidth = 100; // MHz
    bwp->m_transmissionMode = 1;   // SISO

    allBwps.push_back(bwp);

    nrHelper.SetBwps(allBwps);

    NetDeviceContainer gnbDevs = nrHelper.InstallGnbDevice(gnbNodes, epcHelper.GetGnbBearer());
    NetDeviceContainer ueDevs = nrHelper.InstallUeDevice(ueNodes, epcHelper.GetUserEquipmentBearer());

    // Attach UEs to the EPC
    epcHelper.Attach(ueDevs);

    // Assign IP addresses
    Ipv4InterfaceContainer gnbIpIfaces;
    Ipv4InterfaceContainer ueIpIface = epcHelper.AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default gateway for UEs
    for (uint32_t j = 0; j < ueNodes.GetN(); ++j)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(
            ueNodes.Get(j)->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper.GetUeDefaultGatewayAddress(), 1);
    }

    // Install TCP server on gNodeB
    uint16_t sinkPort = 8000;
    Address sinkAddress(InetSocketAddress(ueIpIface.GetAddress(0), sinkPort)); // Dummy address
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(gnbNodes.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime));

    // Install TCP client on UE
    OnOffHelper clientApp("ns3::TcpSocketFactory", sinkPort);
    clientApp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientApp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientApp.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    clientApp.SetAttribute("PacketSize", UintegerValue(512));
    clientApp.SetAttribute("MaxBytes", UintegerValue(5 * 512)); // Send five packets

    ApplicationContainer clientApps = clientApp.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    // Enable logging
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("nr-tcp-random-walk-ue.tr");
    nrHelper.EnableTraces();
    epcHelper.EnableTraces();

    // Flow monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Print flow statistics
    monitor->CheckForLostPackets();
    flowmon.SerializeToXmlFile("nr-tcp-random-walk-ue.flowmon", false, false);

    Simulator::Destroy();

    return 0;
}