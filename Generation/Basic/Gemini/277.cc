#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/nr-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;
using namespace ns3::nr;

int main(int argc, char *argv[])
{
    // Set up logging (optional, useful for debugging)
    LogComponentEnable("NrHelper", LOG_LEVEL_INFO);
    LogComponentEnable("MobilityHelper", LOG_LEVEL_INFO);
    LogComponentEnable("RandomWalk2dMobilityModel", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);

    // Create nodes for gNodeB and UE
    NodeContainer nrNodes;
    nrNodes.Create(2); // One gNB, one UE
    Ptr<Node> gnbNode = nrNodes.Get(0);
    Ptr<Node> ueNode = nrNodes.Get(1);

    // Install Internet Stack on nodes
    InternetStackHelper internet;
    internet.Install(nrNodes);

    // Configure NR network helper
    NrHelper nrHelper;
    nrHelper.SetBand(Band::n78); // Example NR band
    nrHelper.SetNumerology(Numerology::N2); // Example numerology (30kHz subcarrier spacing)
    nrHelper.SetPolarization(Polarization::Dual); // Example polarization

    // Install NR devices on gNB and UE nodes
    NetDeviceContainer gnbDevs = nrHelper.InstallGnbDevice(gnbNode);
    NetDeviceContainer ueDevs = nrHelper.InstallUeDevice(ueNode);

    // Assign IP addresses to the NR interfaces
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer nrInterfaces = ipv4.Assign(NetDeviceContainer(gnbDevs.Get(0), ueDevs.Get(0)));

    Ipv4Address gnbIp = nrInterfaces.GetAddress(0);
    // Ipv4Address ueIp = nrInterfaces.GetAddress(1); // UE IP is not directly used for TCP client setup, gnbIp is needed

    // Configure Mobility Models
    MobilityHelper mobility;

    // gNodeB: Static position at (0,0,0)
    mobility.SetMobilityModelType("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNode);
    gnbNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // UE: Random Walk mobility within a 100x100 area
    mobility.SetMobilityModelType("ns3::RandomWalk2dMobilityModel");
    mobility.SetInitialAttribute("Bounds", RectangleValue(Rectangle(0, 100, 0, 100))); // Set bounds for the random walk
    mobility.SetInitialAttribute("Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); // 1 m/s speed
    mobility.SetInitialAttribute("Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // No pause time
    
    mobility.Install(ueNode);

    // Set initial random position for UE within the 100x100 area
    Ptr<RandomWalk2dMobilityModel> ueMobility = ueNode->GetObject<RandomWalk2dMobilityModel>();
    Ptr<UniformRandomVariable> posRv = CreateObject<UniformRandomVariable>();
    posRv->SetAttribute("Min", DoubleValue(0.0));
    posRv->SetAttribute("Max", DoubleValue(100.0));
    ueMobility->SetPosition(Vector(posRv->GetValue(), posRv->GetValue(), 0.0));

    // Create TCP Server on gNodeB
    uint16_t port = 9; // Standard Echo Port, or any other port
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sinkHelper.Install(gnbNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Create TCP Client on UE
    // The UE should send five 512-byte packets at 1-second intervals to the gNodeB.
    TcpClientHelper clientHelper(gnbIp, port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(5));      // Send exactly five packets
    clientHelper.SetAttribute("PacketSize", UintegerValue(512));    // Each packet is 512 bytes
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Send a packet every 1 second
    ApplicationContainer clientApps = clientHelper.Install(ueNode);
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(10.0)); // Ensure client stops by end of simulation

    // Set simulation duration
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}