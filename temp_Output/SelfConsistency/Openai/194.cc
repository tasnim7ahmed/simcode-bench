/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-helper.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TwoApMultiStaWifiUdpSimulation");

int 
main (int argc, char *argv[])
{
    uint32_t nStasPerAp = 3;
    double simTime = 10.0; // seconds
    uint32_t payloadSize = 1024; // bytes

    CommandLine cmd;
    cmd.AddValue ("nStasPerAp", "Number of STAs per AP", nStasPerAp);
    cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue ("simTime", "Simulated time", simTime);
    cmd.Parse (argc, argv);

    // Create node containers
    NodeContainer wifiApNodes;
    wifiApNodes.Create (2);

    std::vector<NodeContainer> staNodesVec(2);

    for (uint32_t i = 0; i < 2; ++i)
    {
        staNodesVec[i].Create (nStasPerAp);
    }

    // Configure Wi-Fi channel and phy
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssids[2] = {Ssid ("ap1-ssid"), Ssid ("ap2-ssid")};

    // Net device containers
    std::vector<NetDeviceContainer> staDevicesVec(2);
    NetDeviceContainer apDevices;

    // AP
    for (uint32_t i = 0; i < 2; ++i)
    {
        mac.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssids[i]));
        NetDeviceContainer apDev = wifi.Install (phy, mac, NodeContainer (wifiApNodes.Get (i)));
        apDevices.Add (apDev);

        // STAs
        mac.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssids[i]),
                     "ActiveProbing", BooleanValue (false));
        NetDeviceContainer staDevs = wifi.Install (phy, mac, staNodesVec[i]);
        staDevicesVec[i] = staDevs;
    }

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    // Position APs
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (20.0, 0.0, 0.0));

    // Position STAs per AP
    for (uint32_t apIdx = 0; apIdx < 2; ++apIdx)
    {
        double apX = apIdx == 0 ? 0.0 : 20.0;
        for (uint32_t j = 0; j < nStasPerAp; ++j)
        {
            // Place STAs around each AP
            positionAlloc->Add (Vector (apX + 2.0 + 2.0*j, 5.0, 0.0));
        }
    }

    mobility.SetPositionAllocator (positionAlloc);

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNodes);

    NodeContainer allStas;
    for (uint32_t i=0; i < 2; ++i)
        allStas.Add (staNodesVec[i]);
    mobility.Install (allStas);

    // Internet stack
    InternetStackHelper stack;
    stack.Install (wifiApNodes);
    for (uint32_t i = 0; i < 2; ++i)
    {
        stack.Install (staNodesVec[i]);
    }

    // Assign IP addresses (different subnets for each Wi-Fi segment)
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> staIfVec(2);
    Ipv4InterfaceContainer apIfs;
    for (uint32_t i = 0; i < 2; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << (i+1) << ".0";
        address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
        staIfVec[i] = address.Assign (staDevicesVec[i]);
        Ipv4InterfaceContainer apIf = address.Assign (NetDeviceContainer(apDevices.Get (i)));
        apIfs.Add (apIf);
    }

    // UDP Server on each AP (port 4000 + apIndex)
    std::vector<uint16_t> serverPorts = {4001, 4002};
    std::vector<ApplicationContainer> serverApps(2);
    for (uint32_t apIdx = 0; apIdx < 2; ++apIdx)
    {
        UdpServerHelper server (serverPorts[apIdx]);
        ApplicationContainer app = server.Install (wifiApNodes.Get (apIdx));
        app.Start (Seconds (0.0));
        app.Stop (Seconds (simTime));
        serverApps[apIdx] = app;
    }

    // UDP Client on each STA, sending to its AP server
    std::vector<std::vector<ApplicationContainer>> clientAppsVec(2);

    for (uint32_t apIdx = 0; apIdx < 2; ++apIdx)
    {
        clientAppsVec[apIdx].resize (nStasPerAp);
        for (uint32_t staIdx = 0; staIdx < nStasPerAp; ++staIdx)
        {
            UdpClientHelper client (apIfs.GetAddress (apIdx), serverPorts[apIdx]);
            client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
            client.SetAttribute ("Interval", TimeValue (Seconds (0.01))); // 100 packets/sec
            client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            ApplicationContainer app = client.Install (staNodesVec[apIdx].Get (staIdx));
            app.Start (Seconds (1.0));
            app.Stop (Seconds (simTime));
            clientAppsVec[apIdx][staIdx] = app;
        }
    }

    // Enable PCAP and ASCII tracing
    phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    for (uint32_t apIdx = 0; apIdx < 2; ++apIdx)
    {
        for (uint32_t staIdx = 0; staIdx < nStasPerAp; ++staIdx)
        {
            std::ostringstream ossSta, ossAp;
            ossSta << "sta" << apIdx+1 << "-" << staIdx+1 << ".pcap";
            ossAp << "ap" << apIdx+1 << ".pcap";
            phy.EnablePcap (ossSta.str (), staDevicesVec[apIdx].Get (staIdx), true, true);
        }
        std::ostringstream ossAp;
        ossAp << "ap" << apIdx+1 << ".pcap";
        phy.EnablePcap (ossAp.str (), apDevices.Get (apIdx), true, true);
    }

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("wifi-tracing.tr");
    phy.EnableAsciiAll (stream);

    // Run simulation
    Simulator::Stop (Seconds (simTime + 1));
    Simulator::Run ();

    // Print total data sent by each station
    std::cout << "----- Data Sent Per Station (bytes) -----" << std::endl;
    for (uint32_t apIdx = 0; apIdx < 2; ++apIdx)
    {
        for (uint32_t staIdx = 0; staIdx < nStasPerAp; ++staIdx)
        {
            Ptr<Application> app = clientAppsVec[apIdx][staIdx].Get (0);
            Ptr<UdpClient> client = DynamicCast<UdpClient> (app);
            uint32_t sent = client->GetSent () * payloadSize;
            std::cout << "AP " << (apIdx+1) << " STA " << (staIdx+1) 
                      << " sent: " << sent << " bytes" << std::endl;
        }
    }

    Simulator::Destroy ();
    return 0;
}