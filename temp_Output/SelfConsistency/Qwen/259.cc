#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MmWave5GSimulation");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create the gNB and UEs
    NodeContainer gnbs;
    gnbs.Create(1);

    NodeContainer ues;
    ues.Create(2);

    // Setup mobility for UEs with random walk
    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                                "Distance", DoubleValue(1.0));
    mobilityUe.Install(ues);

    // Setup fixed position for gNB
    MobilityHelper mobilityGnb;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // Stationary gNB at origin
    mobilityGnb.SetPositionAllocator(positionAlloc);
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbs);

    // mmWave helper setup
    mmwave::MmWaveHelper mmwaveHelper;
    mmwaveHelper.Initialize();

    NetDeviceContainer gnbdDevs;
    NetDeviceContainer ueDevs;

    gnbdDevs = mmwaveHelper.InstallGnbDevice(gnbs);
    ueDevs = mmwaveHelper.InstallUeDevice(ues);

    // Attach UEs to gNB
    mmwaveHelper.AttachToClosestGnb(ueDevs, gnbdDevs);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ues);
    internet.Install(gnbs);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer ueInterfaces;
    Ipv4InterfaceContainer gnbInterfaces;

    for (auto &dev : ueDevs)
    {
        Ipv4InterfaceContainer iface = ipv4.Assign(dev);
        ueInterfaces.Add(iface);
        ipv4.NewNetwork();
    }

    for (auto &dev : gnbdDevs)
    {
        Ipv4InterfaceContainer iface = ipv4.Assign(dev);
        gnbInterfaces.Add(iface);
        ipv4.NewNetwork();
    }

    // Set up UDP Echo Server on UE 2 (index 1)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(ues.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Echo Client on UE 0 (index 0)
    UdpEchoClientHelper echoClient(ueInterfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(ues.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Simulation setup
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}