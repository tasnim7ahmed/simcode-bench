#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for TCP and LTE if needed (optional)
    // LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);
    // LogComponentEnable("LteUeRrc", LOG_LEVEL_INFO);

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    NodeContainer ueNodes;

    enbNodes.Create(1);
    ueNodes.Create(1);

    // Create the LTE helper and set up the downlink and uplink carriers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to the first eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set up TCP server and client applications
    uint16_t dlPort = 12345;

    // Server (on eNodeB node) listens on TCP port
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(enbNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Client (on UE node) sends TCP traffic to eNodeB
    OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(ueIpIfaces.GetAddress(0), dlPort));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("MaxBytes", UintegerValue(0));  // Infinite packets

    ApplicationContainer clientApp = client.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}