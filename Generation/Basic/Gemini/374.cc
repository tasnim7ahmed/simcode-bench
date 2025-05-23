#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-client-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/constant-position-mobility-model.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Configure default LTE bandwidths for simplicity
    Config::SetDefault("ns3::LteUeNetDevice::UlBandwidth", UintegerValue(50));
    Config::SetDefault("ns3::LteUeNetDevice::DlBandwidth", UintegerValue(50));

    // Create LTE and EPC helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<EpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create a single eNB node (Node 1)
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Create a single UE node (Node 2)
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install LTE devices on the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the nodes (essential for applications and routing)
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Attach UE to eNB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Activate default EPS bearer for the UE
    // This step is crucial for establishing data connectivity and assigning an IP address to the UE.
    lteHelper->ActivateDataRadioBearer(ueLteDevs, enbLteDevs);

    // Mobility setup
    // Node 1 (eNB): Constant Position Mobility
    MobilityHelper enbMobility;
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(0.0, 0.0, 0.0)); // Place eNB at the origin
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.SetMobilityModelType("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    // Node 2 (UE): Random Walk 2D Mobility
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModelType("ns3::RandomWalk2dMobilityModel");
    ueMobility.Set("Bounds", RectangleValue(Rectangle(-250, 250, -250, 250))); // Movement bounds for UE
    ueMobility.Set("Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]")); // UE moves at 5 m/s
    ueMobility.Install(ueNodes);

    // Applications: Node 2 (UE) sends UDP packets to Node 1 (eNB)
    uint16_t sinkPort = 9; // Common port for discard service or a custom application

    // Get the IP address of the eNB's interface connected to the EPC/S1-U for the Packet Sink
    // The eNB node will have an Ipv4 interface connected to the PointToPointEpcHelper created by lteHelper.
    Ptr<Node> enbNode = enbNodes.Get(0);
    Ptr<Ipv4> enbIpv4 = enbNode->GetObject<Ipv4>();
    Ipv4Address enbAppAddress;
    for (uint32_t i = 0; i < enbIpv4->GetNInterfaces(); ++i)
    {
        if (enbIpv4->GetAddress(i, 0).GetLocal() != Ipv4Address::GetLoopback())
        {
            // Assuming the first non-loopback address is the one used for external connectivity (S1-U)
            enbAppAddress = enbIpv4->GetAddress(i, 0).GetLocal();
            break;
        }
    }

    // Packet Sink (server) on Node 1 (eNB)
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(enbAppAddress, sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(enbNode);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // UDP Client (source) on Node 2 (UE)
    UdpClientHelper udpClient(enbAppAddress, sinkPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10000));     // Send up to 10,000 packets
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100))); // Send a packet every 100ms
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));     // Packet size of 1024 bytes
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0)); // Start client after 1 second to allow network setup time
    clientApps.Stop(Seconds(9.0));  // Stop client before simulation ends

    // Populate global routing tables (required for communication through the EPC)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set simulation duration and run
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}