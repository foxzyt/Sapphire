import random

subjects = ["eu", "você", "ele", "ela", "nós", "eles", "elas", "o cachorro", "o gato", "o pássaro", "o menino", "a menina", "o homem", "a mulher", "o professor", "a médica", "o engenheiro", "a cantora", "o motorista", "a advogada"]
verbs = ["gosta de", "adora", "odeia", "ama", "prefere", "quer", "precisa de", "tem", "comprou", "vendeu", "fez", "criou", "descobriu", "olhou para", "encontrou", "perdeu", "escondeu", "mostrou", "entregou", "recebeu"]
objects = ["maçãs", "bananas", "carros", "livros", "computadores", "celulares", "filmes", "músicas", "jogos", "viagens", "dinheiro", "tempo", "amigos", "problemas", "soluções", "histórias", "segredos", "mentiras", "verdades", "sonhos"]
adverbs = ["hoje", "ontem", "amanhã", "sempre", "nunca", "às vezes", "frequentemente", "raramente", "rápido", "devagar", "bem", "mal", "muito", "pouco", "bastante", "demais", "agora", "depois", "cedo", "tarde"]
places = ["na escola", "em casa", "no trabalho", "na rua", "no parque", "na praia", "no cinema", "no teatro", "no restaurante", "no hospital", "no banco", "na loja", "no mercado", "no shopping", "na fazenda", "na cidade", "no campo", "no país", "no exterior", "no mundo"]

phrases = set()
while len(phrases) < 500:
    structure = random.choice([1, 2, 3, 4, 5])
    
    s = random.choice(subjects)
    v = random.choice(verbs)
    o = random.choice(objects)
    a = random.choice(adverbs)
    p = random.choice(places)
    
    if structure == 1:
        phrase = f"{s} {v} {o} ."
    elif structure == 2:
        phrase = f"{s} {v} {o} {a} ."
    elif structure == 3:
        phrase = f"{s} {v} {o} {p} ."
    elif structure == 4:
        phrase = f"{a} , {s} {v} {o} ."
    else:
        phrase = f"{s} {v} {o} {p} {a} ."
        
    # ensure it's a valid space separated phrase for our simple tokenizer
    # replace multiple spaces with single space
    phrase = " ".join(phrase.split())
    phrases.add(phrase)

phrases = list(phrases)[:500]

with open("500_phrases.txt", "w", encoding="utf-8") as f:
    for p in phrases:
        f.write(f'    listAppend(dataset, "{p}");\n')

print(f"Generated {len(phrases)} phrases.")
