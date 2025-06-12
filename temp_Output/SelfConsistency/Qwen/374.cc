#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpSimulation");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("LteUdpSimulation", LOG_LEVEL_INFO);

    // Create nodes: Node 0 (eNB), Node 1 (UE)
    NodeContainer enbNode;
    enbNode.Create(1);
    NodeContainer ueNode;
    ueNode.Create(1);

    // Create and set up LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Install LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNode);

    // Install internet stack on UE
    InternetStackHelper internet;
    internet.Install(ueNode);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UE to eNB
    lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

    // Set up UDP server on eNB side (port 8000)
    UdpServerHelper server(8000);
    ApplicationContainer serverApp = server.Install(enbNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on UE side
    UdpClientHelper client(ueIpIface.GetAddress(0), 8000);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(ueNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Mobility setup
    MobilityHelper mobilityEnb;
    mobilityEnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(0.0),
                                     "DeltaY", DoubleValue(0.0),
                                     "GridWidth", UintegerValue(1),
                                     "LayoutType", StringValue("RowFirst"));
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNode);

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                                "Distance", DoubleValue(5.0));
    mobilityUe.Install(ueNode.Get(0));

    // Enable PCAP tracing for LTE
    lteHelper->EnablePcap("lte_udp_simulation_enb", enbLteDevs.Get(0));
    lteHelper->EnablePcap("lte_udp_simulation_ue", ueLteDevs.Get(0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}