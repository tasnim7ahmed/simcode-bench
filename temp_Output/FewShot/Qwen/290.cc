#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable LTE tracing for PHY, MAC, and RLC layers
    Config::SetDefault("ns3::LteHelper::UseRlcSm", BooleanValue(true));
    Config::SetDefault("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue(false));

    // Create eNodeB and UE node containers
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(1);

    // Install mobility for UE (random movement)
    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobilityUe.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobilityUe.Install(ueNodes);

    // Set constant position for eNodeB
    MobilityHelper mobilityEnb;
    mobilityEnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(50.0),
                                     "MinY", DoubleValue(50.0),
                                     "DeltaX", DoubleValue(0.0),
                                     "DeltaY", DoubleValue(0.0),
                                     "GridWidth", UintegerValue(1),
                                     "LayoutType", StringValue("RowFirst"));
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set up UDP server on UE
    uint16_t port = 5000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on eNodeB
    Ipv4Address ueIpAddr = ueIpIface.GetAddress(0);
    UdpClientHelper udpClient(ueIpAddr, port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(0)); // Infinite until simulation ends
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(enbNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable LTE traces
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();
    lteHelper->EnableRlcTraces();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}