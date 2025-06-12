#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: one eNodeB and one UE
    NodeContainer enbNodes;
    NodeContainer ueNodes;

    enbNodes.Create(1);
    ueNodes.Create(1);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Install LTE devices
    NetDeviceContainer enbDevs;
    NetDeviceContainer ueDevs;

    enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to eNodeBs
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set TCP as default protocol for UE
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    // Setup applications: TCP sink on eNodeB, TCP source on UE
    uint16_t dlPort = 12345;

    // Install TCP sink (server) on eNodeB
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    PacketSinkHelper dlPacketSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = dlPacketSinkHelper.Install(enbNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install TCP source (client) on UE
    AddressValue remoteAddress(InetSocketAddress(ueIpIface.GetAddress(0), dlPort));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));
    OnOffHelper client("ns3::TcpSocketFactory", Address());
    client.SetAttribute("Remote", remoteAddress);
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    lteHelper->EnablePcap("lte-simple-tcp", enbDevs.Get(0));
    lteHelper->EnablePcap("lte-simple-tcp", ueDevs.Get(0));

    // Set up simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}