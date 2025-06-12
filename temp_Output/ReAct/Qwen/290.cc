#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    double simTime = 10.0;
    double interPacketInterval = 0.2; // seconds
    uint32_t packetSize = 1024;

    Config::SetDefault("ns3::LteHelper::UseRlcSm", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::AnrEnabled", BooleanValue(false));
    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));

    // Enable LTE traces
    Config::SetDefault("ns3::LteEnbPhy::EnableUplinkTrace", BooleanValue(true));
    Config::SetDefault("ns3::LteEnbPhy::EnableDownlinkTrace", BooleanValue(true));
    Config::SetDefault("ns3::LteEnbMac::EnableUplinkTrace", BooleanValue(true));
    Config::SetDefault("ns3::LteEnbMac::EnableDownlinkTrace", BooleanValue(true));
    Config::SetDefault("ns3::LteRlcAm::LogAll", BooleanValue(true));
    Config::SetDefault("ns3::LteRlcUm::LogAll", BooleanValue(true));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Mobility configuration
    MobilityHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                                "Distance", DoubleValue(5.0));
    mobilityUe.Install(ueNodes);

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UE to eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Install IP stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Setup applications
    UdpServerHelper server(5000);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper client(ueIpIface.GetAddress(0), 5000);
    client.SetAttribute("MaxPackets", UintegerValue(static_cast<uint32_t>(simTime / interPacketInterval)));
    client.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(simTime));

    // Enable PCAP tracing for all LTE devices
    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}