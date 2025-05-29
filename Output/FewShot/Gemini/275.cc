#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create LTE Helper
    LteHelper lteHelper;

    // Create eNB Node
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Create UE Node
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Configure eNB
    NetDeviceContainer enbDevs;
    enbDevs = lteHelper.InstallEnbDevice(enbNodes);

    // Configure UE
    NetDeviceContainer ueDevs;
    ueDevs = lteHelper.InstallUeDevice(ueNodes);

    // Mobility Model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("2s"),
                              "Speed", StringValue("1m/s"),
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));

    mobility.Install(ueNodes);

    Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel>();
    enbNodes.Get(0)->AggregateObject(enbMobility);
    Vector3D position(50, 50, 0);
    enbMobility->SetPosition(position);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Assign IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

    // Attach UE to eNB
    lteHelper.Attach(ueDevs, enbDevs.Get(0));

    // Set up Simplex UDP Server on eNB
    uint16_t port = 9; // Discard port
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up Simplex UDP Client on UE
    UdpClientHelper client(enbIpIface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Run Simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}