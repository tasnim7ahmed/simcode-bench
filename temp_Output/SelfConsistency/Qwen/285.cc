#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteMobileUdpSimulation");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(3);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set pathloss model
    lteHelper->SetPathlossModelType(TypeId::FindByName("ns3::FriisSpectrumPropagationLossModel"));

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes.Get(0));

    mobility.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(ueNodes);

    // Setup applications
    uint16_t port = 5000;

    // UDP Server on UE 0
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Client on UE 1
    UdpClientHelper client(ueIpIface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = client.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    PointToPointHelper p2ph;
    p2ph.EnablePcapAll("lte-mobile-udp");

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}