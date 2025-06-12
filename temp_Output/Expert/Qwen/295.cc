#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for LTE components if needed
    // LogComponentEnable("LteHelper", LOG_LEVEL_INFO);

    double simTime = 10.0;
    uint16_t packetSize = 1024;
    Time interPacketInterval = MilliSeconds(100);

    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::UseIdealRlc", BooleanValue(true));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Mobility configuration
    MobilityHelper mobilityEnb;
    mobilityEnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(0.0),
                                     "DeltaY", DoubleValue(0.0),
                                     "GridWidth", UintegerValue(1),
                                     "LayoutType", StringValue("RowFirst"));
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobilityUe.Install(ueNodes);

    // Install LTE devices
    NetDeviceContainer enbDevs;
    NetDeviceContainer ueDevs;

    enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to the closest eNB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Setup applications
    uint16_t dlPort = 12345;

    // UDP Server (UE)
    UdpServerHelper server(dlPort);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Client (eNodeB)
    UdpClientHelper client(ueIpIfaces.GetAddress(0), dlPort);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(simTime));

    // Set simulation time and run
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}