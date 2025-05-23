#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/netanim-module.h"
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWiredWirelessSimulation");

// Global variables for command line arguments
uint32_t numBackboneRouters = 3;
uint32_t numLanNodes = 2;
uint32_t numStaNodes = 3;
double simTime = 60.0;
bool tracePcap = true;
bool traceAscii = false;
bool enableCourseChangeCallback = true;
bool enableNetAnim = false;

// Callback for mobility model course change
void CourseChange(std::string context, Ptr<const MobilityModel> model)
{
    Vector pos = model->GetPosition();
    Vector vel = model->GetVelocity();
    NS_LOG_INFO(context << " pos=" << pos.x << ", " << pos.y << ", " << pos.z << "; vel=" << vel.x << ", " << vel.y << ", " << vel.z);
}

int main(int argc, char *argv[])
{
    // Configure logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("OlsrRoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable("MixedWiredWirelessSimulation", LOG_LEVEL_INFO);

    // Command line arguments parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("numBackboneRouters", "Number of backbone routers", numBackboneRouters);
    cmd.AddValue("numLanNodes", "Number of LAN nodes per router", numLanNodes);
    cmd.AddValue("numStaNodes", "Number of STA nodes per AP", numStaNodes);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("tracePcap", "Enable PCAP tracing", tracePcap);
    cmd.AddValue("traceAscii", "Enable ASCII tracing", traceAscii);
    cmd.AddValue("enableCourseChangeCallback", "Enable mobility course change callback", enableCourseChangeCallback);
    cmd.AddValue("enableNetAnim", "Enable NetAnim output", enableNetAnim);
    cmd.Parse(argc); // Parse command line arguments

    // Validate inputs
    if (numBackboneRouters == 0)
    {
        NS_FATAL_ERROR("Number of backbone routers must be greater than 0");
    }

    // --- Node Creation ---
    NodeContainer backboneRouters;
    backboneRouters.Create(numBackboneRouters);

    // Store LAN and STA nodes in vectors of NodeContainers
    std::vector<NodeContainer> lanNodes(numBackboneRouters);
    std::vector<NodeContainer> staNodes(numBackboneRouters);

    NodeContainer allAccessNodes; // Collect all non-backbone nodes for InternetStack installation
    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        lanNodes[i].Create(numLanNodes);
        allAccessNodes.Add(lanNodes[i]);
        staNodes[i].Create(numStaNodes);
        allAccessNodes.Add(staNodes[i]);
    }

    // --- Install Internet Stack on all nodes ---
    InternetStackHelper internet;
    internet.Install(backboneRouters);
    internet.Install(allAccessNodes);

    // --- Install OLSR routing on backbone routers ---
    OlsrHelper olsrHelper;
    olsrHelper.Install(backboneRouters);

    // Containers to hold all devices for tracing
    NetDeviceContainer backboneAdhocDevices;
    std::vector<NetDeviceContainer> lanDevices(numBackboneRouters);
    std::vector<NetDeviceContainer> apDevices(numBackboneRouters);
    std::vector<NetDeviceContainer> staDevices(numBackboneRouters);

    // IP address management
    Ipv4AddressHelper address;

    // --- Backbone Ad Hoc WiFi Network (OLSR domain) ---
    YansWifiChannelHelper channelBackbone = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyBackbone = YansWifiPhyHelper::Default();
    phyBackbone.SetChannel(channelBackbone.Create());

    WifiHelper wifiBackbone;
    wifiBackbone.SetStandard(WIFI_STANDARD_80211a);
    wifiBackbone.SetRemoteStationManager(
        "ns3::AarfWifiManager",
        "Threshold", StringValue("20.0 dBm"));

    WifiMacHelper macBackbone;
    macBackbone.SetType("ns3::AdhocWifiMac",
                        "Ssid", SsidValue("BackboneAdhocSsid"));

    backboneAdhocDevices = wifiBackbone.Install(phyBackbone, macBackbone, backboneRouters);
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer backboneAdhocInterfaces = address.Assign(backboneAdhocDevices);

    // --- Loop for each backbone router to setup local networks ---
    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        Ptr<Node> currentRouter = backboneRouters.Get(i);

        // --- Local LAN (CSMA) ---
        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NodeContainer lanDevicesContainer;
        lanDevicesContainer.Add(currentRouter); // Router is part of this LAN
        lanDevicesContainer.Add(lanNodes[i]);   // Add local LAN nodes

        lanDevices[i] = csma.Install(lanDevicesContainer);

        address.SetBase("192.168." + std::to_string(i + 1) + ".0", "255.255.255.0");
        Ipv4InterfaceContainer lanInterfaces = address.Assign(lanDevices[i]);

        // Set default route for LAN nodes to their router
        // Router is at index 0 of lanDevices[i] and lanInterfaces
        // LAN nodes are at indices 1 to numLanNodes of lanDevices[i] and lanInterfaces
        Ipv4Address gatewayIp = lanInterfaces.GetAddress(0); // Router's IP on this LAN

        for (uint32_t j = 0; j < numLanNodes; ++j)
        {
            Ptr<Node> lanNode = lanNodes[i].Get(j);
            Ptr<NetDevice> lanNetDevice = lanDevices[i].Get(j + 1); // Get the device for this LAN node
            Ptr<Ipv4> ipv4LanNode = lanNode->GetObject<Ipv4>();
            Ptr<Ipv4RoutingProtocol> routingProtocol = ipv4LanNode->GetRoutingProtocol();
            Ptr<Ipv4ListRouting> listRouting = DynamicCast<Ipv4ListRouting>(routingProtocol);
            if (listRouting)
            {
                Ptr<Ipv4StaticRouting> staticRouting = listRouting->GetStaticRouting();
                staticRouting->AddDefaultRoute(gatewayIp, lanNetDevice->GetIfIndex());
            }
        }

        // --- Local 802.11 Infrastructure WiFi (AP + STAs) ---
        YansWifiChannelHelper channelInfra = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phyInfra = YansWifiPhyHelper::Default();
        phyInfra.SetChannel(channelInfra.Create());

        WifiHelper wifiInfra;
        wifiInfra.SetStandard(WIFI_STANDARD_80211n_5GHZ);
        wifiInfra.SetRemoteStationManager(
            "ns3::AarfWifiManager",
            "Threshold", StringValue("20.0 dBm"));

        // AP MAC
        WifiMacHelper macAp;
        Ssid ssidAp = Ssid("InfraSsid" + std::to_string(i + 1));
        macAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssidAp));
        apDevices[i] = wifiInfra.Install(phyInfra, macAp, currentRouter);

        // STA MAC
        WifiMacHelper macSta;
        macSta.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssidAp));
        staDevices[i] = wifiInfra.Install(phyInfra, macSta, staNodes[i]);

        address.SetBase("172.16." + std::to_string(i + 1) + ".0", "255.255.255.0");
        Ipv4InterfaceContainer apInterface = address.Assign(apDevices[i]);
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices[i]);

        // Set default route for STA nodes to their AP
        // AP is at index 0 of apDevices[i] and apInterface
        // STAs are at indices 0 to numStaNodes-1 of staDevices[i] and staInterfaces
        gatewayIp = apInterface.GetAddress(0); // AP's IP on this Infra WiFi

        for (uint32_t j = 0; j < numStaNodes; ++j)
        {
            Ptr<Node> staNode = staNodes[i].Get(j);
            Ptr<NetDevice> staNetDevice = staDevices[i].Get(j); // Get the device for this STA node
            Ptr<Ipv4> ipv4StaNode = staNode->GetObject<Ipv4>();
            Ptr<Ipv4RoutingProtocol> routingProtocol = ipv4StaNode->GetRoutingProtocol();
            Ptr<Ipv4ListRouting> listRouting = DynamicCast<Ipv4ListRouting>(routingProtocol);
            if (listRouting)
            {
                Ptr<Ipv4StaticRouting> staticRouting = listRouting->GetStaticRouting();
                staticRouting->AddDefaultRoute(gatewayIp, staNetDevice->GetIfIndex());
            }
        }
    }

    // --- Mobility Configuration ---
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(-500, 500, -500, 500)),
                               "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                               "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

    mobility.Install(backboneRouters);
    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        mobility.Install(staNodes[i]);
    }

    if (enableCourseChangeCallback)
    {
        for (uint32_t i = 0; i < backboneRouters.GetN(); ++i)
        {
            Ptr<Node> node = backboneRouters.Get(i);
            Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
            if (mob)
            {
                std::string context = "Node " + std::to_string(node->GetId()) + " (Router " + std::to_string(i) + ")";
                mob->TraceConnectWithoutContext("CourseChange", MakeBoundCallback(&CourseChange, context));
            }
        }
        for (uint32_t i = 0; i < numBackboneRouters; ++i)
        {
            for (uint32_t j = 0; j < staNodes[i].GetN(); ++j)
            {
                Ptr<Node> node = staNodes[i].Get(j);
                Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
                if (mob)
                {
                    std::string context = "Node " + std::to_string(node->GetId()) + " (STA in Infra " + std::to_string(i) + ")";
                    mob->TraceConnectWithoutContext("CourseChange", MakeBoundCallback(&CourseChange, context));
                }
            }
        }
    }

    // --- UDP Flow Setup ---
    Ptr<Node> udpSourceNode = lanNodes[0].Get(0); // First node of the first LAN
    Ptr<Node> udpSinkNode = staNodes[numBackboneRouters - 1].Get(0); // First STA of the last AP

    Ipv4Address sinkAddress = staDevices[numBackboneRouters - 1].Get(0)->GetAddress(0).GetLocal();
    uint16_t port = 9; // Discard port

    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(udpSinkNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper udpClient(sinkAddress, port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(udpSourceNode);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // --- Tracing ---
    if (tracePcap)
    {
        phyBackbone.EnablePcapAll("mixed-wired-wireless-backbone");
        csma.EnablePcap("mixed-wired-wireless-lan-first", lanDevices[0].Get(0), true); // Router part of first LAN
        csma.EnablePcap("mixed-wired-wireless-lan-first-node", lanDevices[0].Get(1), true); // First node of first LAN
        phyInfra.EnablePcap("mixed-wired-wireless-ap-last", apDevices[numBackboneRouters - 1].Get(0), true);
        phyInfra.EnablePcap("mixed-wired-wireless-sta-last", staDevices[numBackboneRouters - 1].Get(0), true);
    }

    if (traceAscii)
    {
        AsciiTraceHelper asciiTraceHelper;
        Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mixed-wired-wireless.tr");
        internet.EnableAsciiIpv4Info(stream); // Enables for all nodes with InternetStack
        wifiBackbone.EnableAsciiAll(stream);
        for(uint32_t i = 0; i < numBackboneRouters; ++i)
        {
            csma.EnableAsciiAll(stream, lanDevices[i]);
            wifiInfra.EnableAsciiAll(stream, apDevices[i]);
            wifiInfra.EnableAsciiAll(stream, staDevices[i]);
        }
    }

    if (enableNetAnim)
    {
        AnimationInterface anim("mixed-wired-wireless.xml");
        anim.SetConstantPosition(backboneRouters.Get(0), 0.0, 0.0);
        anim.EnablePacketMetadata();
        anim.EnableIpv4RouteTracking("routing-table.xml", Seconds(0), Seconds(simTime), Seconds(0.1));
    }

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}