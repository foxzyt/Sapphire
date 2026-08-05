#!/usr/bin/env python3
# contar_linhas.py - Conta linhas de código em diretórios especificados

import os
import sys
import argparse
from pathlib import Path
from collections import defaultdict
import fnmatch

def contar_linhas_arquivo(caminho):
    """Conta linhas de um arquivo"""
    try:
        with open(caminho, 'r', encoding='utf-8', errors='ignore') as f:
            return sum(1 for _ in f)
    except Exception:
        return 0

def contar_linhas_diretorio(diretorio, extensoes=None, ignorar_dirs=None, ignorar_arquivos=None):
    """
    Conta linhas em um diretório
    
    Args:
        diretorio: Caminho do diretório
        extensoes: Lista de extensões para filtrar (ex: ['.py', '.cpp'])
        ignorar_dirs: Lista de nomes de diretórios para ignorar
        ignorar_arquivos: Lista de padrões de arquivos para ignorar
    """
    if not os.path.exists(diretorio):
        print(f"⚠️  Diretório não encontrado: {diretorio}")
        return 0, {}
    
    total = 0
    por_extensao = defaultdict(int)
    por_arquivo = {}
    
    ignorar_dirs = ignorar_dirs or []
    ignorar_arquivos = ignorar_arquivos or []
    
    for root, dirs, files in os.walk(diretorio):
        # Remove diretórios ignorados
        dirs[:] = [d for d in dirs if d not in ignorar_dirs]
        
        for file in files:
            caminho = Path(root) / file
            
            # Verifica se deve ignorar
            ignorar = False
            for padrao in ignorar_arquivos:
                if fnmatch.fnmatch(file, padrao):
                    ignorar = True
                    break
            
            if ignorar:
                continue
            
            # Verifica extensão
            ext = caminho.suffix
            if extensoes and ext not in extensoes:
                continue
            
            linhas = contar_linhas_arquivo(caminho)
            total += linhas
            por_extensao[ext if ext else '(sem extensão)'] += linhas
            por_arquivo[str(caminho)] = linhas
    
    return total, por_extensao, por_arquivo

def main():
    parser = argparse.ArgumentParser(
        description='Conta linhas de código em diretórios',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Exemplos:
  python contar_linhas.py src
  python contar_linhas.py src include tests -e .cpp .h
  python contar_linhas.py src --ignorar build __pycache__ --ignorar-arquivos "*.pyc" "*.o"
  python contar_linhas.py src include -e .py --ignorar tests
        """
    )
    
    parser.add_argument(
        'diretorios',
        nargs='+',
        help='Diretórios para analisar'
    )
    
    parser.add_argument(
        '-e', '--extensoes',
        nargs='+',
        default=[],
        help='Extensões para contar (ex: .cpp .h .py)'
    )
    
    parser.add_argument(
        '--ignorar',
        nargs='+',
        default=[],
        help='Nomes de diretórios para ignorar (ex: build __pycache__)'
    )
    
    parser.add_argument(
        '--ignorar-arquivos',
        nargs='+',
        default=[],
        help='Padrões de arquivos para ignorar (ex: "*.pyc" "*.o")'
    )
    
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Mostra detalhes por arquivo'
    )
    
    parser.add_argument(
        '--por-extensao',
        action='store_true',
        help='Mostra apenas resumo por extensão'
    )
    
    parser.add_argument(
        '--minimo',
        type=int,
        default=0,
        help='Linhas mínimas para mostrar um arquivo (default: 0)'
    )
    
    args = parser.parse_args()
    
    total_geral = 0
    ext_geral = defaultdict(int)
    total_arquivos = 0
    
    # Cabeçalho
    print("=" * 60)
    print("📊 CONTADOR DE LINHAS")
    print("=" * 60)
    
    if args.extensoes:
        print(f"📁 Extensões: {', '.join(args.extensoes)}")
    else:
        print("📁 Extensões: todas")
    
    if args.ignorar:
        print(f"🚫 Ignorando diretórios: {', '.join(args.ignorar)}")
    
    if args.ignorar_arquivos:
        print(f"🚫 Ignorando arquivos: {', '.join(args.ignorar_arquivos)}")
    
    print()
    
    for diretorio in args.diretorios:
        print(f"📂 Analisando: {diretorio}")
        print("-" * 40)
        
        total, por_extensao, por_arquivo = contar_linhas_diretorio(
            diretorio,
            args.extensoes,
            args.ignorar,
            args.ignorar_arquivos
        )
        
        total_geral += total
        total_arquivos += len(por_arquivo)
        
        for ext, count in por_extensao.items():
            ext_geral[ext] += count
        
        # Mostra arquivos individuais (se verboso)
        if args.verbose:
            for arquivo, linhas in sorted(por_arquivo.items(), key=lambda x: x[1], reverse=True):
                if linhas >= args.minimo:
                    print(f"  {linhas:6d} - {arquivo}")
        elif not args.por_extensao:
            print(f"  Total: {total} linhas em {len(por_arquivo)} arquivos")
        
        # Mostra por extensão
        if args.por_extensao and por_extensao:
            print("\n  📊 Por extensão:")
            for ext, count in sorted(por_extensao.items(), key=lambda x: x[1], reverse=True):
                print(f"    {ext}: {count} linhas")
        
        print()
    
    # Resumo final
    print("=" * 60)
    print("📊 RESUMO FINAL")
    print("=" * 60)
    print(f"📁 Diretórios analisados: {len(args.diretorios)}")
    print(f"📄 Arquivos analisados: {total_arquivos}")
    print(f"📝 TOTAL DE LINHAS: {total_geral}")
    
    if ext_geral:
        print("\n📊 Por extensão:")
        for ext, count in sorted(ext_geral.items(), key=lambda x: x[1], reverse=True):
            print(f"  {ext}: {count} linhas ({count/total_geral*100:.1f}%)")
    
    print("=" * 60)

if __name__ == "__main__":
    main()